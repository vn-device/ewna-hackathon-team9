#!/usr/bin/env python3
import time
from arduino.app_utils import App, Bridge

# Industrial Gesture Action Protocol
IDLE    = 0  # Dim Cobalt Monitoring
PAUSE   = 1  # Solid Amber (Raised Palm)
CONFIRM = 2  # Solid Green (Thumbs Up)
SKIP    = 3  # Process Cyan Chase (Swipe L -> R)
FAULT   = 4  # Strobe Emergency Red (Unrecognized / Low Conf.)

STATE_DESCRIPTIONS = {
    IDLE:    "MONITORING / IDLE   -> Dim Cobalt Baseline, Silent",
    PAUSE:   "PAUSE / HOLD        -> Solid Amber, Caution Tone (440 Hz)",
    CONFIRM: "CONFIRM / ACK       -> Solid Green, Rising Chime (523 Hz -> 659 Hz)",
    SKIP:    "SKIP / ADVANCE      -> Process Cyan Chase, Rapid Pip (880 Hz)",
    FAULT:   "REJECT / FAULT      -> Emergency Red Strobe, Error Buzz (220 Hz)"
}

def fire_action(code: int, conf: int = 100) -> None:
    """Dispatches a discrete gesture action to the real-time MCU over the RPC bridge."""
    desc = STATE_DESCRIPTIONS.get(code, "UNKNOWN")
    print(f"[IPC TX] set_action(code={code}, conf={conf}%) | {desc}")
    Bridge.call("set_action", code, conf)

def update_tracking(conf: int) -> None:
    """Updates the live LED confidence bar on the real-time MCU."""
    print(f"[IPC TX] set_tracking(conf={conf}%)")
    Bridge.call("set_tracking", conf)

def loop() -> None:
    """
    Hardware-in-the-loop simulation cycle.
    Validates RPC message delivery, LED bar interpolation, and buzzer timing.
    """
    print("\n--- Starting Modulino State Simulation Cycle ---")

    # 1. Simulate active hand approach (Confidence Bar Ramp)
    print("\n[Stage 1] Simulating hand entry and tracking confidence ramp...")
    for confidence in range(10, 101, 15):
        update_tracking(confidence)
        time.sleep(0.12)

    time.sleep(0.3)

    # 2. Cycle through each functional industrial state defined in the README
    test_states = [PAUSE, CONFIRM, SKIP, FAULT]

    for state in test_states:
        print(f"\n[Stage 2] Triggering state: {STATE_DESCRIPTIONS[state]}")
        fire_action(state, conf=95)
        
        # Hold state display to inspect physical Modulino nodes
        time.sleep(2.0)

        # Return to monitoring baseline between actions
        print("[Stage 2] Reverting to monitoring baseline...")
        fire_action(IDLE, conf=0)
        time.sleep(1.2)

    print("\n--- Simulation Cycle Complete. Restarting loop in 2 seconds... ---")
    time.sleep(2.0)

# Entry point for the Arduino Linux runtime
App.run(user_loop=loop)