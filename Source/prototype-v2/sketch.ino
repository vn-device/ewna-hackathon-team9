#include <Arduino_RouterBridge.h>
#include <Modulino.h>

ModulinoPixels pixels;
ModulinoBuzzer  buzzer;

// ---------- colors ----------
ModulinoColor C_OFF  (0,   0,   0);
ModulinoColor C_GREY (60,  60,  60);
ModulinoColor C_PAUSE(255, 140, 0);    // amber
ModulinoColor C_OK   (0,   255, 0);    // green
ModulinoColor C_SKIP (0,   80,  255);  // blue

const uint8_t NUM_PIX     = 8;
const uint8_t BRIGHT      = 25;
const uint8_t BRIGHT_IDLE = 8;

// ---------- timing ----------
const unsigned long BLINK_MS    = 500;   // idle blink period (half)
const unsigned long TRACK_TO_MS = 500;   // no tracking update -> back to idle
const unsigned long HOLD_MS     = 2000;  // gesture stays lit
const unsigned long SWEEP_MS    = 45;    // per-pixel swipe step
const unsigned long FADE_MS     = 60;    // per-pixel fade step

// ---------- state machine ----------
enum State { IDLE, TRACKING, SWEEP, SHOWING, FADE };
State state = IDLE;

unsigned long stateMs = 0, lastStepMs = 0, lastBlinkMs = 0;
bool blinkOn = false;
int  step = 0, dir = 1;
ModulinoColor animColor = C_SKIP;

// ---------- helpers ----------
void fill(ModulinoColor c, uint8_t b) {
  for (uint8_t i = 0; i < NUM_PIX; i++) pixels.set(i, c, b);
  pixels.show();
}

void bar(ModulinoColor c, uint8_t n, uint8_t b) {
  for (uint8_t i = 0; i < NUM_PIX; i++)
    pixels.set(i, i < n ? c : C_OFF, i < n ? b : 0);
  pixels.show();
}

uint8_t confToPixels(int conf) {
  int n = (conf * NUM_PIX) / 100;
  if (n < 1) n = 1;
  if (n > NUM_PIX) n = NUM_PIX;
  return (uint8_t)n;
}

void enterIdle() {
  state = IDLE;
  blinkOn = false;
  lastBlinkMs = 0;              // blink immediately
  fill(C_OFF, 0);
}

// ---------- Bridge handlers ----------

// Nothing detected
void setIdle() {
  enterIdle();
}

// Hand seen, no gesture committed yet: solid grey confidence bar
void setTracking(int conf) {
  if (state == SWEEP || state == SHOWING || state == FADE) return;  // don't interrupt
  state = TRACKING;
  stateMs = millis();
  bar(C_GREY, confToPixels(conf), BRIGHT);
}

// Static gesture: 1 = pause, 2 = confirm. Bar length = confidence
void setAction(int code, int conf) {
  dir = 1;                      // fade left-to-right afterward
  if (code == 1) {
    bar(C_PAUSE, confToPixels(conf), BRIGHT);
    buzzer.tone(330, 250);                                  // low, long
  } else {
    bar(C_OK, confToPixels(conf), BRIGHT);
    buzzer.tone(880, 100); delay(80); buzzer.tone(880, 100); // double high
  }
  state = SHOWING;
  stateMs = millis();
}

// Swipe: +1 = left-to-right, -1 = right-to-left
void setSwipe(int d) {
  dir = d;
  animColor = C_SKIP;
  step = (dir > 0) ? 0 : NUM_PIX - 1;
  fill(C_OFF, 0);
  if (dir > 0) { buzzer.tone(500, 80); delay(40); buzzer.tone(750, 80); }  // rising
  else         { buzzer.tone(750, 80); delay(40); buzzer.tone(500, 80); }  // falling
  state = SWEEP;
  lastStepMs = millis();
}

// ---------- setup / loop ----------
void setup() {
  Modulino.begin();
  pixels.begin();
  buzzer.begin();

  Bridge.begin();
  Bridge.provide("set_idle",     setIdle);
  Bridge.provide("set_tracking", setTracking);
  Bridge.provide("set_action",   setAction);
  Bridge.provide("set_swipe",    setSwipe);

  enterIdle();
}

void loop() {
  unsigned long now = millis();

  switch (state) {

    case IDLE:                                     // grey blink
      if (now - lastBlinkMs >= BLINK_MS) {
        lastBlinkMs = now;
        blinkOn = !blinkOn;
        fill(blinkOn ? C_GREY : C_OFF, blinkOn ? BRIGHT_IDLE : 0);
      }
      break;

    case TRACKING:                                 // hand lost -> idle
      if (now - stateMs >= TRACK_TO_MS) enterIdle();
      break;

    case SWEEP:                                    // light follows the hand
      if (now - lastStepMs >= SWEEP_MS) {
        lastStepMs = now;
        pixels.set(step, animColor, BRIGHT);
        pixels.show();
        step += dir;
        if (step < 0 || step >= NUM_PIX) { state = SHOWING; stateMs = now; }
      }
      break;

    case SHOWING:                                  // hold, then fade
      if (now - stateMs >= HOLD_MS) {
        state = FADE;
        step = (dir > 0) ? 0 : NUM_PIX - 1;
        lastStepMs = now;
      }
      break;

    case FADE:                                     // off in the same direction
      if (now - lastStepMs >= FADE_MS) {
        lastStepMs = now;
        pixels.set(step, C_OFF, 0);
        pixels.show();
        step += dir;
        if (step < 0 || step >= NUM_PIX) enterIdle();
      }
      break;
  }

  delay(5);
}
