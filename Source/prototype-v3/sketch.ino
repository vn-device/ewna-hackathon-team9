#include <Arduino_RouterBridge.h>
#include <Arduino_Modulino.h>

ModulinoPixels pixels;
ModulinoBuzzer buzzer;

const ModulinoColor C_OFF(0, 0, 0);
const ModulinoColor C_IDLE(0, 15, 80);
const ModulinoColor C_PAUSE(255, 140, 0);
const ModulinoColor C_OK(0, 255, 0);
const ModulinoColor C_SKIP(0, 220, 255);
const ModulinoColor C_FAULT(255, 0, 0);

const uint8_t NUM_PIX = 8;
const uint8_t BRIGHT = 30;
const uint8_t BRIGHT_LO = 6;

int currentAction = 0;
unsigned long lastCmdMs = 0;
const unsigned long HOLD_MS = 2000;

// Non-blocking Tone Sequencer state
struct ToneStep
{
    uint16_t freq;
    uint16_t duration;
    uint16_t pauseAfter;
};

ToneStep toneQueue[4];
uint8_t toneQueueLen = 0;
uint8_t toneQueueIdx = 0;
unsigned long nextToneStepMs = 0;

// Idle breathing pulse state
unsigned long lastBreathMs = 0;
int breathLevel = 2;
int breathDir = 1;

// Non-blocking directional LED wave state
bool isWiping = false;
int8_t wipeIndex = 0;
int8_t wipeStep = 1;
unsigned long nextWipeStepMs = 0;
const uint16_t WIPE_DELAY_MS = 35;

// Non-blocking strobe state
bool isStrobing = false;
uint8_t strobeFlashes = 0;
bool strobeOn = false;
unsigned long nextStrobeMs = 0;
const uint16_t STROBE_DELAY_MS = 80;

void fill(const ModulinoColor& c, uint8_t b)
{
    for (uint8_t i = 0; i < NUM_PIX; i++)
    {
        pixels.set(i, c, b);
    }
    pixels.show();
}

void bar(const ModulinoColor& c, uint8_t n)
{
    for (uint8_t i = 0; i < NUM_PIX; i++)
    {
        pixels.set(i, (i < n) ? c : C_OFF, BRIGHT);
    }
    pixels.show();
}

ModulinoColor colorFor(int code)
{
    switch (code)
    {
        case 1:
            return C_PAUSE;
        case 2:
            return C_OK;
        case 3:
        case 4:
            return C_SKIP;
        case 5:
            return C_FAULT;
        default:
            return C_IDLE;
    }
}

void queueToneSequence(int code)
{
    toneQueueIdx = 0;
    switch (code)
    {
        case 1:
            toneQueue[0] = {330, 250, 0};
            toneQueueLen = 1;
            break;

        case 2:
            toneQueue[0] = {523, 100, 40};
            toneQueue[1] = {659, 150, 0};
            toneQueueLen = 2;
            break;

        case 3:
        case 4:
            toneQueue[0] = {880, 80, 0};
            toneQueueLen = 1;
            break;

        case 5:
            toneQueue[0] = {220, 300, 0};
            toneQueueLen = 1;
            break;

        default:
            toneQueueLen = 0;
            buzzer.noTone();
            break;
    }
    nextToneStepMs = millis();
}

void processToneSequencer()
{
    if (toneQueueIdx < toneQueueLen && millis() >= nextToneStepMs)
    {
        ToneStep step = toneQueue[toneQueueIdx];
        buzzer.tone(step.freq, step.duration);
        nextToneStepMs = millis() + step.duration + step.pauseAfter;
        toneQueueIdx++;
    }
}

void triggerWipe(int8_t stepDirection)
{
    isWiping = true;
    wipeStep = stepDirection;

    if (wipeStep > 0)
    {
        wipeIndex = 0;
    }
    else
    {
        wipeIndex = NUM_PIX - 1;
    }
    nextWipeStepMs = millis();
}

void processWipeSequence()
{
    if (isWiping && millis() >= nextWipeStepMs)
    {
        pixels.clear();
        pixels.set(wipeIndex, C_SKIP, BRIGHT);
        pixels.show();

        wipeIndex += wipeStep;

        if (wipeIndex >= NUM_PIX || wipeIndex < 0)
        {
            isWiping = false;
            pixels.clear();
            pixels.show();
        }
        else
        {
            nextWipeStepMs = millis() + WIPE_DELAY_MS;
        }
    }
}

void triggerStrobe()
{
    isStrobing = true;
    strobeFlashes = 0;
    strobeOn = true;
    nextStrobeMs = millis();
}

void processStrobeSequence()
{
    if (isStrobing && millis() >= nextStrobeMs)
    {
        if (strobeOn)
        {
            fill(C_FAULT, BRIGHT);
        }
        else
        {
            pixels.clear();
            pixels.show();
            strobeFlashes++;
        }

        strobeOn = !strobeOn;

        if (strobeFlashes >= 3)
        {
            isStrobing = false;
        }
        else
        {
            nextStrobeMs = millis() + STROBE_DELAY_MS;
        }
    }
}

void setAction(int code, int conf)
{
    currentAction = code;
    lastCmdMs = millis();

    isWiping = false;
    isStrobing = false;

    if (code == 0)
    {
        fill(C_IDLE, BRIGHT_LO);
        return;
    }

    if (code == 3)
    {
        triggerWipe(1);
    }
    else if (code == 4)
    {
        triggerWipe(-1);
    }
    else if (code == 5)
    {
        triggerStrobe();
    }
    else
    {
        uint8_t n = (uint8_t)((conf * NUM_PIX) / 100);
        if (n < 1)
        {
            n = 1;
        }
        if (n > NUM_PIX)
        {
            n = NUM_PIX;
        }
        bar(colorFor(code), n);
    }

    queueToneSequence(code);
}

void setTracking(int conf)
{
    if (currentAction != 0)
    {
        return;
    }
    lastCmdMs = millis();

    uint8_t n = (uint8_t)((conf * NUM_PIX) / 100);
    for (uint8_t i = 0; i < NUM_PIX; i++)
    {
        pixels.set(i, (i < n) ? C_IDLE : C_OFF, BRIGHT);
    }
    pixels.show();
}

void setup()
{
    Modulino.begin();
    pixels.begin();
    buzzer.begin();
    fill(C_IDLE, BRIGHT_LO);

    Bridge.begin();
    Bridge.provide("set_action", setAction);
    Bridge.provide("set_tracking", setTracking);
    lastCmdMs = millis();
}

void loop()
{
    unsigned long now = millis();

    processToneSequencer();
    processWipeSequence();
    processStrobeSequence();

    if (currentAction != 0 && (now - lastCmdMs > HOLD_MS))
    {
        currentAction = 0;
        isWiping = false;
        isStrobing = false;
        fill(C_IDLE, BRIGHT_LO);
    }

    if (currentAction == 0 && !isWiping && !isStrobing && (now - lastBreathMs > 40))
    {
        lastBreathMs = now;
        breathLevel += breathDir;
        if (breathLevel >= 14 || breathLevel <= 2)
        {
            breathDir = -breathDir;
        }
        fill(C_IDLE, (uint8_t)breathLevel);
    }
}