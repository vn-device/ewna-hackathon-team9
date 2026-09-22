from arduino.app_utils import App, Bridge
import time

PAUSE, CONFIRM = 1, 2
L2R, R2L = 1, -1
IDLE_SECONDS = 5

def idle():
    """Nothing in frame: grey blink."""
    Bridge.call("set_idle")
    time.sleep(IDLE_SECONDS)

def track_ramp():
    """Hand enters frame: confidence climbs before a gesture commits."""
    for conf in range(20, 90, 10):
        Bridge.call("set_tracking", conf)
        time.sleep(0.1)

def loop():
    idle(); track_ramp(); Bridge.call("set_swipe", R2L);          time.sleep(3)
    idle(); track_ramp(); Bridge.call("set_swipe", L2R);          time.sleep(3)
    idle(); track_ramp(); Bridge.call("set_action", PAUSE, 85);   time.sleep(3)
    idle(); track_ramp(); Bridge.call("set_action", CONFIRM, 95); time.sleep(3)

App.run(user_loop=loop)
