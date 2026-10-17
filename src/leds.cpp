#include "leds.h"
#include <FastLED.h>
#include "IdleAnimation.h"
#include "LedColor.h"
#include "config.h"

static CRGB leds[LED_COUNT];
static uint8_t shownMask = 0xFF;  // nothing drawn yet
static uint32_t lastFrameMs = 0;
static uint32_t idleSinceMs = 0;
static bool fadeDone = false;  // latched so the fade can't replay when millis() wraps

static void drawIdle(uint32_t nowMs) {
    const uint8_t fade = fadeDone ? 255 : idleFade(nowMs - idleSinceMs, IDLE_FADE_MS);
    fadeDone = fade == 255;
    for (uint16_t i = 0; i < LED_COUNT; i++) {
        const Rgb c = scaleRgb(idlePixel(STATION_ID, i, LED_COUNT, nowMs), fade);
        leds[i] = CRGB(c.r, c.g, c.b);
    }
}

static void draw(uint8_t mask, uint32_t nowMs) {
    if (mask == 0) {
        drawIdle(nowMs);
    } else if (mask == ALL_BUTTONS_MASK) {
        for (uint16_t i = 0; i < LED_COUNT; i++) {
            leds[i] = CHSV(rainbowHue(i, LED_COUNT, nowMs), 255, 255);
        }
    } else {
        const Rgb c = maskColor(mask);
        fill_solid(leds, LED_COUNT, CRGB(c.r, c.g, c.b));
    }
    FastLED.show();
    shownMask = mask;
    lastFrameMs = nowMs;
}

void leds_begin() {
    FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, LED_COUNT);
    FastLED.setBrightness(LED_BRIGHTNESS);
    leds_update(0, millis());
}

void leds_update(uint8_t mask, uint32_t nowMs) {
    if (mask == 0 && shownMask != 0) {  // just released (or booting): fade idle in
        idleSinceMs = nowMs;
        fadeDone = false;
    }
    // Solid colors only redraw on change; idle and rainbow animate at LED_FRAME_MS.
    const bool animated = mask == 0 || mask == ALL_BUTTONS_MASK;
    if (mask != shownMask || (animated && nowMs - lastFrameMs >= LED_FRAME_MS)) draw(mask, nowMs);
}

void leds_progress(uint32_t done, uint32_t total) {
    const uint16_t lit = total ? uint64_t(done) * LED_COUNT / total : 0;
    static uint16_t shownLit = UINT16_MAX;
    if (lit == shownLit) return;  // progress fires every ~1.4 KB; only redraw on change
    shownLit = lit;
    fill_solid(leds, LED_COUNT, CRGB::Black);
    fill_solid(leds, lit, CRGB(0, 0, 80));
    FastLED.show();
    shownMask = 0xFF;  // force a redraw if the upload fails and the loop resumes
}
