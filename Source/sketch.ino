#include <Arduino.h>
#include <Modulino.h>

ModulinoPixels pixels;
ModulinoBuzzer buzzer;

// Modulino Industrial Andon Color Palette from README
const ModulinoColor C_PAUSE(255, 140, 0); // Solid Industrial Amber
const ModulinoColor C_OK(0, 255, 0);      // Solid Pure Green
const ModulinoColor C_SKIP(0, 220, 255);  // Process Cyan
const ModulinoColor C_IDLE(0, 15, 80);    // Dim Cobalt Baseline
const ModulinoColor C_FAULT(255, 0, 0);   // Emergency Red
const ModulinoColor C_OFF(0, 0, 0);

const uint8_t NUM_PIX = 8;
const uint8_t BRIGHT_STD = 30; // 30% brightness to keep bus current within specs
const uint8_t BRIGHT_DIM = 8;  // Baseline idle brightness

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

    clearPixels();
    delay(500);
}

void loop()
{
    // =========================================================================
    // State 1: PAUSE / HOLD (Solid Industrial Amber, Low Caution Tone 440 Hz)
    // =========================================================================
    setAllPixels(C_PAUSE, BRIGHT_STD);
    buzzer.tone(440, 250);
    delay(1500);

    // =========================================================================
    // State 2: CONFIRM / ACK (Solid Pure Green, Rising Chime 523 Hz -> 659 Hz)
    // =========================================================================
    setAllPixels(C_OK, BRIGHT_STD);
    buzzer.tone(523, 100);
    delay(120);
    buzzer.tone(659, 150);
    delay(1500);

    // =========================================================================
    // State 3: SKIP / ADVANCE (Process Cyan Chase, Rapid Pip 880 Hz)
    // =========================================================================
    buzzer.tone(880, 80);
    // Dynamic lateral swipe animation across all 8 LEDs
    for (uint8_t loopCount = 0; loopCount < 2; loopCount++)
    {
        for (uint8_t i = 0; i < NUM_PIX; i++)
        {
            pixels.clear();
            pixels.set(i, C_SKIP, BRIGHT_STD);
            pixels.show();
            delay(40);
        }
    }
    clearPixels();
    delay(1500);

    // =========================================================================
    // State 4: MONITORING / IDLE (Dim Cobalt Baseline, Silent)
    // =========================================================================
    setAllPixels(C_IDLE, BRIGHT_DIM);
    buzzer.noTone();
    delay(2000);

    // =========================================================================
    // State 5: REJECT / FAULT (Strobe Emergency Red, Error Buzz 220 Hz)
    // =========================================================================
    buzzer.tone(220, 300);
    for (uint8_t flash = 0; flash < 3; flash++)
    {
        setAllPixels(C_FAULT, BRIGHT_STD);
        delay(80);
        clearPixels();
        delay(80);
    }
    delay(1500);
}