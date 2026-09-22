#include <Arduino_RouterBridge.h>
#include <Modulino.h>

ModulinoPixels pixels;
ModulinoBuzzer  buzzer;

// Gesture colors
ModulinoColor C_OFF  (0,   0,   0);
ModulinoColor C_IDLE (0,   0,   60);   // dim blue — alive, waiting
ModulinoColor C_PAUSE(255, 140, 0);    // amber
ModulinoColor C_OK   (0,   255, 0);    // green
ModulinoColor C_SKIP (0,   80,  255);  // blue

const uint8_t NUM_PIX   = 8;
const uint8_t BRIGHT    = 25;
const uint8_t BRIGHT_LO = 6;

int           currentAction = 0;   // 0=idle 1=pause 2=confirm 3=skip
unsigned long lastCmdMs     = 0;
const unsigned long HOLD_MS = 2000;   // revert to idle if Linux goes quiet

// --- idle breathing pulse state ---
unsigned long lastBreathMs = 0;
int   breathLevel = 2;
int   breathDir   = 1;

void fill(ModulinoColor c, uint8_t b) {
  for (uint8_t i = 0; i < NUM_PIX; i++) pixels.set(i, c, b);
  pixels.show();
}

// Light n of 8 pixels in color c — the confidence bar
void bar(ModulinoColor c, uint8_t n) {
  for (uint8_t i = 0; i < NUM_PIX; i++)
    pixels.set(i, i < n ? c : C_OFF, BRIGHT);
  pixels.show();
}

ModulinoColor colorFor(int code) {
  switch (code) {
    case 1:  return C_PAUSE;
    case 2:  return C_OK;
    case 3:  return C_SKIP;
    default: return C_IDLE;
  }
}

void playTone(int code) {
  switch (code) {
    case 1: buzzer.tone(330, 250); break;                    // low, long
    case 2: buzzer.tone(880, 100); delay(80);
            buzzer.tone(880, 100); break;                    // double high beep
    case 3: buzzer.tone(500, 90);  delay(40);
            buzzer.tone(700, 90);  break;                    // rising sweep
    default: break;
  }
}

// Called from Python: code = gesture, conf = 0..100
void setAction(int code, int conf) {
  currentAction = code;
  lastCmdMs     = millis();

  if (code == 0) { fill(C_IDLE, BRIGHT_LO); return; }

  uint8_t n = (uint8_t)((conf * NUM_PIX) / 100);
  if (n < 1) n = 1;
  if (n > NUM_PIX) n = NUM_PIX;

  bar(colorFor(code), n);
  playTone(code);
}

// Live confidence bar while tracking, before an action fires
void setTracking(int conf) {
  if (currentAction != 0) return;      // don't stomp an active gesture
  lastCmdMs = millis();
  uint8_t n = (uint8_t)((conf * NUM_PIX) / 100);
  for (uint8_t i = 0; i < NUM_PIX; i++)
    pixels.set(i, i < n ? C_IDLE : C_OFF, BRIGHT);
  pixels.show();
}

void setup() {
  Modulino.begin();
  pixels.begin();
  buzzer.begin();
  fill(C_IDLE, BRIGHT_LO);

  Bridge.begin();
  Bridge.provide("set_action",   setAction);
  Bridge.provide("set_tracking", setTracking);
  lastCmdMs = millis();
}

void loop() {
  unsigned long now = millis();

  // Watchdog: Linux went quiet -> safe idle state
  if (currentAction != 0 && now - lastCmdMs > HOLD_MS) {
    currentAction = 0;
    fill(C_IDLE, BRIGHT_LO);
  }

  // Breathing idle pulse so "waiting" never looks like "crashed"
  if (currentAction == 0 && now - lastBreathMs > 40) {
    lastBreathMs = now;
    breathLevel += breathDir;
    if (breathLevel >= 12 || breathLevel <= 2) breathDir = -breathDir;
    fill(C_IDLE, breathLevel);
  }

  delay(5);
}
