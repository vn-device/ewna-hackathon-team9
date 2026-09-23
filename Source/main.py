#!/usr/bin/env python3
import os
import sys
import cv2
import time
from edge_impulse_linux.image import ImageImpulseRunner
from arduino.app_utils import App, Bridge

# Protocol Actions
IDLE = 0
PAUSE = 1       # static five (Orange)
GOOD = 2        # thumbs up (Green Flash)
SWIPE_RIGHT = 3 # swipe right (Blue)
SWIPE_LEFT = 4  # swipe left (Blue)

# Cumulative scoreboard counter
stats = \
{
    "five": 0,
    "good": 0,
    "swipe_right": 0,
    "swipe_left": 0
}

def print_scoreboard(triggered: str) -> None:
    print(f"\n{'='*20} GESTURE TRIGGERED: {triggered.upper()} {'='*20}")
    print(f"  [Count] Five (Orange)        : {stats['five']}")
    print(f"  [Count] Good (Green Flash)   : {stats['good']}")
    print(f"  [Count] Swipe Right (Blue)   : {stats['swipe_right']}")
    print(f"  [Count] Swipe Left (Blue)    : {stats['swipe_left']}")
    print(f"{'='*56}\n")

# Model path resolution
dir_path = os.path.dirname(os.path.realpath(__file__))
modelfile = os.path.join(dir_path, '../models/hand-gestures-real.eim')

print(f"Loading model: {modelfile}")
runner = ImageImpulseRunner(modelfile)
try:
    model_info = runner.init()
    print("Model initialized successfully.")
except Exception as e:
    print(f"Failed to initialize model: {e}")
    sys.exit(1)

# Initialize Logitech Brio 100 camera feed
cap = cv2.VideoCapture(0, cv2.CAP_V4L2)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
cap.set(cv2.CAP_PROP_FPS, 30)

# Swipe trajectory buffer
swipe_history = []
SWIPE_WINDOW_SEC = 1.2
SWIPE_MIN_DELTA_X = 0.07  # Sensitive swipe detection (7% frame width)

# Static five timing and position latching
five_first_seen_time = 0.0
last_five_seen_time = 0.0
STATIC_FIVE_DWELL_SEC = 0.40

last_action_time = 0.0
current_stable_action = IDLE
action_candidate_counter = 0

ACTION_DEBOUNCE_SEC = 0.9
CONFIDENCE_THRESHOLD = 0.38
MOTION_TRACK_THRESHOLD = 0.18

def evaluate_motion(now: float):
    global swipe_history
    # Retain points within the sliding time window
    swipe_history = [(t, x) for (t, x) in swipe_history if now - t <= SWIPE_WINDOW_SEC]
    if len(swipe_history) < 2:
        return IDLE, 0.0

    start_x = swipe_history[0][1]
    end_x = swipe_history[-1][1]
    dx = end_x - start_x

    if abs(dx) >= SWIPE_MIN_DELTA_X:
        swipe_history.clear()
        return (SWIPE_RIGHT if dx > 0 else SWIPE_LEFT), abs(dx)
    return IDLE, abs(dx)

def loop() -> None:
    global last_action_time, current_stable_action, action_candidate_counter
    global swipe_history, five_first_seen_time, last_five_seen_time

    if not cap.isOpened():
        time.sleep(0.5)
        return

    ret, frame = cap.read()
    if not ret:
        return

    now = time.time()

    # Mirror horizontally for natural user perspective
    frame = cv2.flip(frame, 1)
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    features, cropped = runner.get_features_from_image(rgb_frame)
    res = runner.classify(features)

    highest_conf = 0.0
    best_label = "none"
    detected_norm_x = None

    if "result" in res:
        if "bounding_boxes" in res["result"] and len(res["result"]["bounding_boxes"]) > 0:
            for bb in res["result"]["bounding_boxes"]:
                score = bb.get("value", 0.0)
                label = bb.get("label", "none")
                if label in ("five", "good") and score > highest_conf:
                    highest_conf = score
                    best_label = label
                    frame_w = frame.shape[1]
                    detected_norm_x = (bb["x"] + (bb["width"] / 2.0)) / frame_w
        elif "classification" in res["result"]:
            for label, score in res["result"]["classification"].items():
                if label in ("five", "good") and score > highest_conf:
                    highest_conf = score
                    best_label = label

    scaled_conf = int(highest_conf * 100)
    print(f"[Debug] Detected: {best_label} | Conf: {highest_conf:.2f} | X: {detected_norm_x}")

    detected_action = IDLE

    # Track open-palm coordinates even across low-confidence motion-blurred frames
    if best_label == "five" and detected_norm_x is not None and highest_conf >= MOTION_TRACK_THRESHOLD:
        swipe_history.append((now, detected_norm_x))
        last_five_seen_time = now
        if five_first_seen_time == 0.0:
            five_first_seen_time = now

        motion_action, total_disp = evaluate_motion(now)
        if motion_action != IDLE:
            detected_action = motion_action
            five_first_seen_time = 0.0
        elif (now - five_first_seen_time >= STATIC_FIVE_DWELL_SEC) and (highest_conf >= CONFIDENCE_THRESHOLD):
            # Only latch static five if the hand was truly still (displacement < 0.05)
            if total_disp < 0.05:
                detected_action = PAUSE
                five_first_seen_time = 0.0
    elif best_label == "good" and highest_conf >= CONFIDENCE_THRESHOLD:
        swipe_history.clear()
        five_first_seen_time = 0.0
        detected_action = GOOD
    else:
        # Tolerate up to 0.35 seconds of frame loss (motion blur/none) before resetting trajectory
        if now - last_five_seen_time > 0.35:
            five_first_seen_time = 0.0
            if now - last_action_time > SWIPE_WINDOW_SEC:
                swipe_history.clear()

    # Dispatch to Arduino sketch
    if detected_action != IDLE:
        if detected_action in (SWIPE_LEFT, SWIPE_RIGHT):
            if now - last_action_time > 0.4:
                action_name = "swipe_right" if detected_action == SWIPE_RIGHT else "swipe_left"
                stats[action_name] += 1
                print_scoreboard(action_name)
                Bridge.call("set_action", detected_action, scaled_conf)
                last_action_time = now
                five_first_seen_time = 0.0
        else:
            if detected_action == current_stable_action:
                action_candidate_counter += 1
            else:
                current_stable_action = detected_action
                action_candidate_counter = 1

            if action_candidate_counter >= 1 and (now - last_action_time > ACTION_DEBOUNCE_SEC):
                action_name = "five" if detected_action == PAUSE else "good"
                stats[action_name] += 1
                print_scoreboard(action_name)
                Bridge.call("set_action", detected_action, scaled_conf)
                last_action_time = now
                action_candidate_counter = 0
                five_first_seen_time = 0.0
    else:
        Bridge.call("set_tracking", 0)
        action_candidate_counter = 0
        current_stable_action = IDLE

App.run(user_loop=loop)