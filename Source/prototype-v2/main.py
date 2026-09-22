from arduino.app_utils import App, Bridge
import time

# Each gesture on the sketch runs its own animation and returns to the
# grey idle blink by itself, so Python only has to trigger it.
SWIPE_SECONDS = 3     # 45 ms x 8 sweep + 2 s hold + 60 ms x 8 fade
SHOW_SECONDS = 3      # 2 s hold + 60 ms x 8 fade
FAULT_SECONDS = 1     # 3 x (80 ms on + 80 ms off)
IDLE_SECONDS = 5      # grey blink between gestures

def gesture(name, seconds):
    """Trigger a gesture, let it finish, then leave the board idle."""
    Bridge.call(name)
    time.sleep(seconds + IDLE_SECONDS)

def loop():
    # Same order as the sketch's DEMO_MODE sequencer
    gesture("swipe_left", SWIPE_SECONDS)   # startSwipe(-1)
    gesture("swipe_right", SWIPE_SECONDS)  # startSwipe(+1)
    gesture("pause", SHOW_SECONDS)         # startPause()
    gesture("confirm", SHOW_SECONDS)       # startConfirm()
    gesture("fault", FAULT_SECONDS)        # startFault()

App.run(user_loop=loop)
