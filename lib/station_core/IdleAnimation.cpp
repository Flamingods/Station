#include "IdleAnimation.h"

// Smooth 0..255..0 wave over one byte of phase (smoothstep on a triangle).
static uint8_t wave8(uint8_t phase) {
    const uint32_t t = phase < 128 ? phase * 2u : (255u - phase) * 2u;
    return uint8_t(t * t * (765u - 2u * t) / 65025u);
}

// Cheap integer hash for deterministic "random" flicker.
static uint8_t hash8(uint32_t x) {
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;
    return uint8_t(x);
}

static uint8_t lerp8(uint8_t a, uint8_t b, uint8_t t) {
    return uint8_t(a + (int(b) - int(a)) * t / 255);
}

static Rgb lerpRgb(Rgb a, Rgb b, uint8_t t) {
    return Rgb{lerp8(a.r, b.r, t), lerp8(a.g, b.g, t), lerp8(a.b, b.b, t)};
}

Rgb scaleRgb(Rgb c, uint8_t level) {
    return Rgb{uint8_t(c.r * level / 255), uint8_t(c.g * level / 255), uint8_t(c.b * level / 255)};
}

uint8_t idleFade(uint32_t sinceMs, uint32_t fadeMs) {
    if (sinceMs >= fadeMs) return 255;
    return uint8_t(sinceMs * 255 / fadeMs);
}

// Station 1: each ember drifts between random heat levels every 120 ms.
static Rgb embers(uint16_t index, uint32_t nowMs) {
    const uint32_t step = nowMs / 120;
    const uint8_t frac = uint8_t((nowMs % 120) * 255 / 120);
    const uint32_t seed = uint32_t(index) * 7919u;
    const uint8_t heat = 60 + lerp8(hash8(seed + step), hash8(seed + step + 1), frac) * 195 / 255;
    return Rgb{heat, uint8_t(heat * heat / 255 * 90 / 255), 0};
}

// Station 2: two waves rolling in opposite directions; crests turn cyan.
static Rgb tide(uint8_t pos, uint32_t nowMs) {
    const uint8_t level = (wave8(uint8_t(pos * 2 + nowMs / 20)) + wave8(uint8_t(pos * 3 - nowMs / 31))) / 2;
    return Rgb{0, uint8_t(level * level / 255 * 200 / 255), uint8_t(80 + level * 175 / 255)};
}

// Station 3: slow color curtains drifting green -> teal -> purple, with shimmer.
static Rgb aurora(uint16_t index, uint8_t pos, uint32_t nowMs) {
    static const Rgb GREEN = {0, 255, 40}, TEAL = {0, 200, 200}, PURPLE = {140, 0, 255};
    const uint8_t t = wave8(uint8_t(pos + nowMs / 40));
    Rgb c = t < 128 ? lerpRgb(GREEN, TEAL, t * 2) : lerpRgb(TEAL, PURPLE, (t - 128) * 2);
    c = scaleRgb(c, 100 + wave8(uint8_t(pos * 2 - nowMs / 27)) * 155 / 255);
    if (hash8(uint32_t(index) * 31u + nowMs / 50) > 250) c = lerpRgb(c, Rgb{255, 255, 255}, 128);
    return c;
}

// Station 4: one comet with an 8-LED tail, alternating magenta and cyan per lap.
static Rgb pulse(uint16_t index, uint16_t count, uint32_t nowMs) {
    if (count == 0) return Rgb{0, 0, 0};
    static const uint8_t TAIL = 8;
    const uint32_t travelled = nowMs / 25;  // 40 LEDs per second
    const uint16_t head = travelled % count;
    const Rgb color = (travelled / count) % 2 ? Rgb{0, 220, 255} : Rgb{200, 0, 255};
    const uint16_t behind = (head + count - index) % count;
    if (behind >= TAIL) return Rgb{0, 0, 0};
    return scaleRgb(color, uint8_t(255 * (TAIL - behind) / TAIL));
}

Rgb idlePixel(uint8_t stationId, uint16_t index, uint16_t count, uint32_t nowMs) {
    const uint8_t pos = count ? uint8_t(uint32_t(index) * 256 / count) : 0;
    Rgb c;
    switch (stationId) {
        case 1: c = embers(index, nowMs); break;
        case 2: c = tide(pos, nowMs); break;
        case 3: c = aurora(index, pos, nowMs); break;
        case 4: c = pulse(index, count, nowMs); break;
        default: return LED_IDLE;
    }
    return scaleRgb(c, IDLE_MAX_LEVEL);
}
