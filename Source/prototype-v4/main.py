#!/usr/bin/env python3
import os
import sys
import cv2
import time
from edge_impulse_linux.image import ImageImpulseRunner
from arduino.app_utils import App, Bridge

# Protocol Actions
IDLE = 0
PAUSE = 1       # five (Orange)
GOOD = 2        # good (Green Flash)
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

# Trajectory window for horizontal swipe detection
swipe_history = []
SWIPE_WINDOW_SEC = 1.0
SWIPE_MIN_DELTA_X = 0.25

last_action_time = 0.0
current_stable_action = IDLE
action_candidate_counter = 0

ACTION_DEBOUNCE_SEC = 1.2
CONFIRMATION_FRAMES = 2
CONFIDENCE_THRESHOLD = 0.60

def detect_swipe(now: float) -> int:
    global swipe_history
    swipe_history = [(t, x) for (t, x) in swipe_history if now - t <= SWIPE_WINDOW_SEC]
    if len(swipe_history) < 3:
        return IDLE

    start_t, start_x = swipe_history[0]
    end_t, end_x = swipe_history[-1]
    dx = end_x - start_x

    if abs(dx) >= SWIPE_MIN_DELTA_X:
        swipe_history.clear()
        if dx > 0:
            return SWIPE_RIGHT
        else:
            return SWIPE_LEFT
    return IDLE

def loop() -> None:
    global last_action_time, current_stable_action, action_candidate_counter, swipe_history

    if not cap.isOpened():
        time.sleep(0.5)
        return

    ret, frame = cap.read()
    if not ret:
        return

    now = time.time()

    # Mirror horizontally for natural perspective
    frame = cv2.flip(frame, 1)
    rgb_frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    features, cropped = runner.get_features_from_image(rgb_frame)
    res = runner.classify(features)

    highest_conf = 0.0
    best_label = "none"
    detected_norm_x = None

    # Parse bounding boxes from object detection output
    # Only target gestures: 'five' and 'good' (everything else, including 'none' and 'neut', is idle)
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

    if highest_conf >= CONFIDENCE_THRESHOLD:
        if best_label == "five":
            if detected_norm_x is not None:
                swipe_history.append((now, detected_norm_x))
                swipe_action = detect_swipe(now)
                if swipe_action != IDLE:
                    detected_action = swipe_action
                else:
                    detected_action = PAUSE
            else:
                detected_action = PAUSE
        elif best_label == "good":
            swipe_history.clear()
            detected_action = GOOD
    else:
        # No target hand gesture in view -> idle
        if now - last_action_time > SWIPE_WINDOW_SEC:
            swipe_history.clear()

    # Dispatch to Arduino sketch
    if detected_action != IDLE:
        if detected_action in (SWIPE_LEFT, SWIPE_RIGHT):
            if now - last_action_time > 0.8:
                action_name = "swipe_right" if detected_action == SWIPE_RIGHT else "swipe_left"
                stats[action_name] += 1
                print_scoreboard(action_name)
                Bridge.call("set_action", detected_action, scaled_conf)
                last_action_time = now
        else:
            if detected_action == current_stable_action:
                action_candidate_counter += 1
            else:
                current_stable_action = detected_action
                action_candidate_counter = 1

            if action_candidate_counter >= CONFIRMATION_FRAMES and (now - last_action_time > ACTION_DEBOUNCE_SEC):
                action_name = "five" if detected_action == PAUSE else "good"
                stats[action_name] += 1
                print_scoreboard(action_name)
                Bridge.call("set_action", detected_action, scaled_conf)
                last_action_time = now
                action_candidate_counter = 0
    else:
        # Keep Modulino solid idle white when nothing is detected
        Bridge.call("set_tracking", 0)
        action_candidate_counter = 0
        current_stable_action = IDLE

App.run(user_loop=loop)