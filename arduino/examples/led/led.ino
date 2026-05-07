#include <Arduino.h>
#include <FastLED.h>
#include <lib8tion.h>

#define NUM_LEDS  1
#define LED_CI_PIN 4
#define LED_DI_PIN  5

CRGB leds[NUM_LEDS] = {0};     // Software gamma mode.

void setup() {
    delay(500); // power-up safety delay
    // Two strips of LEDs, one in HD mode, one in software gamma mode.
    FastLED.addLeds<APA102, LED_CI_PIN, LED_DI_PIN, RGB>(leds, NUM_LEDS);
}

uint8_t wrap_8bit(int i) {
    // Modulo % operator here wraps a large "i" so that it is
    // always in [0, 255] range when returned. For example, if
    // "i" is 256, then this will return 0. If "i" is 257,
    // then this will return 1. No matter how big the "i" is, the
    // output range will always be [0, 255]
    return i % 256;
}

void loop() {
    // Draw a linear ramp of brightnesses to showcase the difference between
    // the HD and non-HD mode.
    for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t brightness = map(i, 0, NUM_LEDS - 1, 0, 255);
        CRGB c(brightness, brightness, brightness);  // Just make a shade of white.
        leds[i] = c;
    }
    FastLED.show();  // All LEDs are now displayed.
    delay(8);  // Wait 8 milliseconds until the next frame.
}
