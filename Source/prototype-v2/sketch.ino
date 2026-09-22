#include <Arduino.h>
#include <Arduino_RouterBridge.h>
#include <Modulino.h>

// 1 = cycle through gestures automatically after 5 s of idle.
// 0 = gestures are only triggered externally (e.g. Python over the Arduino Bridge).
#define DEMO_MODE 0

ModulinoPixels pixels;
ModulinoBuzzer buzzer;

// Modulino Industrial Andon Color Palette from README
const ModulinoColor C_PAUSE(255, 140, 0); // Solid Industrial Amber
const ModulinoColor C_OK(0, 255, 0);      // Solid Pure Green
const ModulinoColor C_SKIP(0, 220, 255);  // Process Cyan
const ModulinoColor C_IDLE(0, 15, 80);    // Dim Cobalt Baseline (reserved for later use)
const ModulinoColor C_FAULT(255, 0, 0);   // Emergency Red
const ModulinoColor C_GREY(60, 60, 60);   // Idle heartbeat blink
const ModulinoColor C_OFF(0, 0, 0);

const uint8_t NUM_PIX = 8;
const uint8_t BRIGHT_STD = 30; // 30% brightness to keep bus current within specs
const uint8_t BRIGHT_DIM = 8;  // Baseline idle brightness

// Timing (ms)
const unsigned long IDLE_BLINK_MS = 500;    // grey on/off period in IDLE
const unsigned long SWEEP_STEP_MS = 45;     // per-pixel light-up during a swipe
const unsigned long HOLD_MS = 2000;         // full-bar hold before fading
const unsigned long FADE_STEP_MS = 60;      // per-pixel turn-off during fade
const unsigned long FAULT_PHASE_MS = 80;    // red flash on/off half-period
const uint8_t FAULT_FLASHES = 3;
const unsigned long GESTURE_TIMEOUT_MS = 8000; // failsafe: force IDLE if a gesture overruns
const unsigned long DEMO_IDLE_MS = 5000;    // idle time before the demo fires the next gesture

enum class State : uint8_t
{
    IDLE,    // grey blink, waiting for a gesture
    SWEEP,   // pixels lighting one at a time
    SHOWING, // full bar held
    FADE,    // pixels turning off one at a time
    FAULT    // red strobe
};

State state = State::IDLE;
unsigned long stateStart = 0;   // millis() when the current state was entered
unsigned long gestureStart = 0; // millis() when the current gesture began (failsafe)
unsigned long lastStep = 0;     // millis() of the last animation step
uint8_t stepCount = 0;          // animation step within the current state
int8_t animDir = 1;             // +1 = pixel 0 -> 7, -1 = pixel 7 -> 0
const ModulinoColor* curColor = &C_OFF;
bool idleLit = false;

void setAllPixels(const ModulinoColor& color, uint8_t brightness)
{
    for (uint8_t i = 0; i < NUM_PIX; i++)
    {
        pixels.set(i, color, brightness);
    }
    pixels.show();
}

void clearPixels()
{
    pixels.clear();
    pixels.show();
}

// Maps the n-th step of an animation to a physical pixel index for the given direction.
uint8_t pixelAt(uint8_t n, int8_t dir)
{
    return (dir > 0) ? n : (NUM_PIX - 1 - n);
}

void enterState(State s)
{
    state = s;
    stateStart = millis();
    lastStep = stateStart;
    stepCount = 0;
}

// =============================================================================
// Gesture start functions: set LEDs/tone, then return immediately.
// =============================================================================

void enterIdle()
{
    buzzer.noTone();
    enterState(State::IDLE);
    idleLit = true;
    setAllPixels(C_GREY, BRIGHT_DIM);
}

// PAUSE / HOLD (Solid Industrial Amber, Low Caution Tone 440 Hz)
void startPause()
{
    gestureStart = millis();
    curColor = &C_PAUSE;
    animDir = 1; // fade left-to-right
    setAllPixels(*curColor, BRIGHT_STD);
    buzzer.tone(440, 250);
    enterState(State::SHOWING);
}

// CONFIRM / ACK (Solid Pure Green, Rising Chime 523 Hz -> 659 Hz)
void startConfirm()
{
    gestureStart = millis();
    curColor = &C_OK;
    animDir = 1; // fade left-to-right
    setAllPixels(*curColor, BRIGHT_STD);
    buzzer.tone(523, 100);
    delay(120);
    buzzer.tone(659, 150);
    enterState(State::SHOWING);
}

// SWIPE (Process Cyan sweep; rising 660 -> 880 Hz for +1, falling 880 -> 660 Hz for -1)
void startSwipe(int dir)
{
    gestureStart = millis();
    curColor = &C_SKIP;
    animDir = (dir >= 0) ? 1 : -1;

    if (animDir > 0)
    {
        buzzer.tone(660, 100);
        delay(120);
        buzzer.tone(880, 150);
    }
    else
    {
        buzzer.tone(880, 100);
        delay(120);
        buzzer.tone(660, 150);
    }

    // Light the first pixel now; loop() lights the rest every SWEEP_STEP_MS.
    pixels.clear();
    pixels.set(pixelAt(0, animDir), *curColor, BRIGHT_STD);
    pixels.show();
    enterState(State::SWEEP);
    stepCount = 1;
}

// REJECT / FAULT (Strobe Emergency Red, Error Buzz 220 Hz)
void startFault()
{
    gestureStart = millis();
    curColor = &C_FAULT;
    buzzer.tone(220, 300);
    setAllPixels(*curColor, BRIGHT_STD); // phase 0 = on
    enterState(State::FAULT);
}

// Zero-argument wrappers so the Bridge can register each swipe direction by name.
void swipeLeft()  { startSwipe(-1); }
void swipeRight() { startSwipe(+1); }

// =============================================================================
// Per-state update functions, called from loop()
// =============================================================================

void runNextDemoGesture()
{
    static uint8_t demoIndex = 0;
    switch (demoIndex)
    {
    case 0: startSwipe(-1); break;
    case 1: startSwipe(+1); break;
    case 2: startPause();   break;
    case 3: startConfirm(); break;
    case 4: startFault();   break;
    }
    demoIndex = (demoIndex + 1) % 5;
}

void updateIdle(unsigned long now)
{
    if (now - lastStep >= IDLE_BLINK_MS)
    {
        lastStep = now;
        idleLit = !idleLit;
        if (idleLit)
        {
            setAllPixels(C_GREY, BRIGHT_DIM);
        }
        else
        {
            clearPixels();
        }
    }

#if DEMO_MODE
    if (now - stateStart >= DEMO_IDLE_MS)
    {
        runNextDemoGesture();
    }
#endif
}

void updateSweep(unsigned long now)
{
    if (now - lastStep < SWEEP_STEP_MS)
    {
        return;
    }
    lastStep = now;

    pixels.set(pixelAt(stepCount, animDir), *curColor, BRIGHT_STD);
    pixels.show();
    stepCount++;

    if (stepCount >= NUM_PIX)
    {
        enterState(State::SHOWING); // hold timer starts once the bar is full
    }
}

void updateShowing(unsigned long now)
{
    if (now - stateStart >= HOLD_MS)
    {
        enterState(State::FADE);
    }
}

void updateFade(unsigned long now)
{
    if (now - lastStep < FADE_STEP_MS)
    {
        return;
    }
    lastStep = now;

    pixels.set(pixelAt(stepCount, animDir), C_OFF, 0);
    pixels.show();
    stepCount++;

    if (stepCount >= NUM_PIX)
    {
        enterIdle();
    }
}

void updateFault(unsigned long now)
{
    if (now - lastStep < FAULT_PHASE_MS)
    {
        return;
    }
    lastStep = now;
    stepCount++;

    if (stepCount >= FAULT_FLASHES * 2)
    {
        enterIdle();
    }
    else if (stepCount % 2 == 0)
    {
        setAllPixels(*curColor, BRIGHT_STD);
    }
    else
    {
        clearPixels();
    }
}

void setup()
{
    Serial.begin(115200);

    // Initialize underlying I2C Wire driver on Qwiic bus
    Modulino.begin();

    if (!pixels.begin())
    {
        Serial.println("Error: Modulino Pixels failed to initialize.");
    }

    if (!buzzer.begin())
    {
        Serial.println("Error: Modulino Buzzer failed to initialize.");
    }

    // Gesture entry points called from main.py over the Arduino Bridge
    Bridge.begin();
    Bridge.provide("swipe_left",  swipeLeft);
    Bridge.provide("swipe_right", swipeRight);
    Bridge.provide("pause",   startPause);
    Bridge.provide("confirm", startConfirm);
    Bridge.provide("fault",   startFault);

    clearPixels();
    delay(500);
    enterIdle();
}

void loop()
{
    unsigned long now = millis();

    // Failsafe: no gesture may run longer than GESTURE_TIMEOUT_MS.
    if (state != State::IDLE && now - gestureStart >= GESTURE_TIMEOUT_MS)
    {
        enterIdle();
        return;
    }

    switch (state)
    {
    case State::IDLE:    updateIdle(now);    break;
    case State::SWEEP:   updateSweep(now);   break;
    case State::SHOWING: updateShowing(now); break;
    case State::FADE:    updateFade(now);    break;
    case State::FAULT:   updateFault(now);   break;
    }
}
