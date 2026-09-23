#!/usr/bin/env python3
import os
import cv2
import time
from edge_impulse_linux.image import ImageImpulseRunner
from arduino.app_utils import App, Bridge

# Industrial Gesture Action Protocol
IDLE  = 0
PAUSE = 1

# Updated mapping matching the trained Edge Impulse label
LABEL_MAP = {
    "pause": PAUSE,
    "background": IDLE,
    "idle": IDLE
}

# Resolve model path relative to the script's directory location
dir_path = os.path.dirname(os.path.realpath(__file__))
modelfile = os.path.join(dir_path, '../models/gesture_model_cpu.eim')

# Initialize Edge Impulse Runner
print(f"Loading model: {modelfile}")
runner = ImageImpulseRunner(modelfile)
try:
    model_info = runner.init()
    print("Model initialized successfully.")
except Exception as e:
    print(f"Failed to initialize model: {e}")
    exit(1)

# Open Logitech BRIO 100 UVC stream
cap = cv2.VideoCapture(2, cv2.CAP_V4L2)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)
cap.set(cv2.CAP_PROP_FPS, 30)

last_action_time = 0.0
current_stable_action = IDLE
action_candidate_counter = 0

ACTION_DEBOUNCE_SEC = 1.2
CONFIRMATION_FRAMES = 3   # Consecutive frames required to latch a classification
CONFIDENCE_THRESHOLD = 0.75

def loop() -> None:
    global last_action_time, current_stable_action, action_candidate_counter

    if not cap.isOpened():
        time.sleep(0.5)
        return

    ret, frame = cap.read()
    if not ret:
        return

    now = time.time()
    
    # Mirror horizontally for natural HMI ergonomics
    # frame = cv2.flip(frame, 1)

    # Convert OpenCV BGR format to standard RGB for the neural network
    frame = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)

    features, cropped = runner.get_features_from_image(frame)
    res = runner.classify(features)
    
    detected_action = IDLE
    highest_conf = 0.0
    best_label = "idle"

    # Parse Edge Impulse classification results
    if "classification" in res["result"]:
        for label, score in res["result"]["classification"].items():
            if score > highest_conf:
                highest_conf = score
                best_label = label

    # Apply threshold and map to MCU action codes
    print(f"[Debug] Label: {best_label} | Conf: {highest_conf:.2f}")

    if highest_conf >= CONFIDENCE_THRESHOLD:
        detected_action = LABEL_MAP.get(best_label, IDLE)

    scaled_conf = int(highest_conf * 100)

    # Handle MCU Communication
    if detected_action != IDLE:
        if detected_action == current_stable_action:
            action_candidate_counter += 1
        else:
            current_stable_action = detected_action
            action_candidate_counter = 1

        if action_candidate_counter >= CONFIRMATION_FRAMES and (now - last_action_time > ACTION_DEBOUNCE_SEC):
            print(f"[Inference] Latched {best_label} ({scaled_conf}%) -> Triggering Action {detected_action}")
            Bridge.call("set_action", detected_action, scaled_conf)
            last_action_time = now
            action_candidate_counter = 0
    else:
        # Stream live confidence tracking to Modulino LED bar
        Bridge.call("set_tracking", scaled_conf if best_label != "idle" else 0)
        action_candidate_counter = 0
        current_stable_action = IDLE

# Bind to the Arduino runtime execution loop
App.run(user_loop=loop)
