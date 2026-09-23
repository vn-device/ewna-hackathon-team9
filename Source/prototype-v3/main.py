#!/usr/bin/env python3
import time
from arduino.app_utils import App, Bridge

# Expanded Industrial Gesture Action Protocol
IDLE     = 0  # Dim Cobalt Monitoring
PAUSE    = 1  # Solid Amber (Raised Palm)
CONFIRM  = 2  # Solid Green (Thumbs Up)
SKIP_LR  = 3  # Process Cyan Wave: Left to Right
SKIP_RL  = 4  # Process Cyan Wave: Right to Left
FAULT    = 5  # Strobe Emergency Red

STATE_DESCRIPTIONS = {
    IDLE:     "MONITORING / IDLE   -> Dim Cobalt Baseline",
    PAUSE:    "PAUSE / HOLD        -> Solid Amber, Caution Tone",
    CONFIRM:  "CONFIRM / ACK       -> Solid Green, Rising Chime",
    SKIP_LR:  "SKIP (Left->Right)  -> Cyan Wave L->R, High Pip",
    SKIP_RL:  "SKIP (Right->Left)  -> Cyan Wave R->L, High Pip",
    FAULT:    "REJECT / FAULT      -> Emergency Red Strobe, Error Buzz"
}

def fire_action(code: int, conf: int = 100) -> None:
    desc = STATE_DESCRIPTIONS.get(code, "UNKNOWN")
    print(f"[IPC TX] set_action(code={code}, conf={conf}%) | {desc}")
    Bridge.call("set_action", code, conf)

def update_tracking(conf: int) -> None:
    print(f"[IPC TX] set_tracking(conf={conf}%)")
    Bridge.call("set_tracking", conf)

def loop() -> None:
    print("\n--- Starting Bidirectional Wave Simulation Cycle ---")

    print("\n[Stage 1] Simulating hand entry and tracking confidence ramp...")
    for confidence in range(10, 101, 20):
        update_tracking(confidence)
        time.sleep(0.12)

    time.sleep(0.3)

    # 2. Cycle through states, notably testing both directional wipes
    test_states = [PAUSE, CONFIRM, SKIP_LR, SKIP_RL, FAULT]

    for state in test_states:
        print(f"\n[Stage 2] Triggering state: {STATE_DESCRIPTIONS[state]}")
        fire_action(state, conf=95)
        
        # Hold state display to observe full wave and tone completion
        time.sleep(2.0)

        print("[Stage 2] Reverting to monitoring baseline...")
        fire_action(IDLE, conf=0)
        time.sleep(1.2)

    print("\n--- Simulation Cycle Complete. Restarting loop in 2 seconds... ---")
    time.sleep(2.0)

App.run(user_loop=loop)