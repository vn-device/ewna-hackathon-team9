from arduino.app_utils import App, Bridge
import time, random

PAUSE, CONFIRM, SKIP, IDLE = 1, 2, 3, 0

def fire(code, conf):
    Bridge.call("set_action", code, conf)

def loop():
    # Fake gesture cycle — replace with real detections tomorrow
    for code in (PAUSE, CONFIRM, SKIP):
        # ramp the confidence bar, as if tracking a hand
        for c in range(20, 95, 15):
            Bridge.call("set_tracking", c)
            time.sleep(0.1)
        fire(code, random.randint(80, 99))
        time.sleep(2.0)
        fire(IDLE, 0)
        time.sleep(1.5)

App.run(user_loop=loop)
