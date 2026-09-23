#include <Arduino_RouterBridge.h>
#include <Arduino_Modulino.h>

ModulinoPixels pixels;
ModulinoBuzzer buzzer;

// Color Definitions
const ModulinoColor C_OFF(0, 0, 0);
const ModulinoColor C_IDLE(180, 180, 180);   // Solid White
const ModulinoColor C_FIVE(255, 120, 0);    // Solid Orange
const ModulinoColor C_GOOD(0, 255, 0);       // Solid Green
const ModulinoColor C_SWIPE(0, 100, 255);    // Solid Blue

const uint8_t NUM_PIX = 8;
const uint8_t BRIGHT_ACTIVE = 40;
const uint8_t BRIGHT_IDLE = 10;

int currentAction = 0;
unsigned long actionStartTime = 0;
const unsigned long HOLD_MS = 1000; // Hold solid gesture color for 1 second

// Non-blocking Tone Sequencer
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

// Flash state (Green Good)
bool isFlashing = false;
uint8_t flashCount = 0;
bool flashOn = false;
unsigned long nextFlashMs = 0;
const uint16_t FLASH_DELAY_MS = 70;

void fillAll(const ModulinoColor& c, uint8_t b)
{
    for (uint8_t i = 0; i < NUM_PIX; i++)
    {
        pixels.set(i, c, b);
    }
    pixels.show();
}

void queueToneSequence(int code)
{
    toneQueueIdx = 0;
    switch (code)
    {
        case 1: // Five (Orange): Low warning tone
            toneQueue[0] = {330, 200, 0};
            toneQueueLen = 1;
            break;

        case 2: // Good (Green): Rising chime
            toneQueue[0] = {523, 80, 30};
            toneQueue[1] = {659, 140, 0};
            toneQueueLen = 2;
            break;

        case 3: // Swipe Right (Blue): High ascending sweep
            toneQueue[0] = {659, 50, 15};
            toneQueue[1] = {880, 50, 15};
            toneQueue[2] = {1046, 90, 0};
            toneQueueLen = 3;
            break;

        case 4: // Swipe Left (Blue): High descending sweep
            toneQueue[0] = {1046, 50, 15};
            toneQueue[1] = {880, 50, 15};
            toneQueue[2] = {659, 90, 0};
            toneQueueLen = 3;
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

void triggerFlash()
{
    isFlashing = true;
    flashCount = 0;
    flashOn = true;
    nextFlashMs = millis();
}

void processFlashSequence()
{
    if (isFlashing && millis() >= nextFlashMs)
    {
        if (flashOn)
        {
            fillAll(C_GOOD, BRIGHT_ACTIVE);
        }
        else
        {
            pixels.clear();
            pixels.show();
            flashCount++;
        }

        flashOn = !flashOn;

        if (flashCount >= 3)
        {
            isFlashing = false;
            fillAll(C_GOOD, BRIGHT_ACTIVE);
        }
        else
        {
            nextFlashMs = millis() + FLASH_DELAY_MS;
        }
    }
}

void setAction(int code, int conf)
{
    (void)conf;
    currentAction = code;
    actionStartTime = millis();
    isFlashing = false;

    if (code == 0)
    {
        fillAll(C_IDLE, BRIGHT_IDLE);
        return;
    }

    if (code == 1) // Five -> Solid Orange
    {
        fillAll(C_FIVE, BRIGHT_ACTIVE);
    }
    else if (code == 2) // Good -> Flash Green
    {
        triggerFlash();
    }
    else if (code == 3 || code == 4) // Swipe -> Solid Blue
    {
        fillAll(C_SWIPE, BRIGHT_ACTIVE);
    }

    queueToneSequence(code);
}

void setTracking(int conf)
{
    (void)conf;
    if (currentAction == 0)
    {
        fillAll(C_IDLE, BRIGHT_IDLE);
    }
}

void setup()
{
    Modulino.begin();
    pixels.begin();
    buzzer.begin();

    fillAll(C_IDLE, BRIGHT_IDLE);

    Bridge.begin();
    Bridge.provide("set_action", setAction);
    Bridge.provide("set_tracking", setTracking);
    actionStartTime = millis();
}

void loop()
{
    unsigned long now = millis();

    processToneSequencer();
    processFlashSequence();

    if (currentAction != 0 && (now - actionStartTime > HOLD_MS))
    {
        currentAction = 0;
        isFlashing = false;
        fillAll(C_IDLE, BRIGHT_IDLE);
    }
}