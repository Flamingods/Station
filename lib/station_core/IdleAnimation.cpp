#include "IdleAnimation.h"
#include "Protocol.h"

// 0..255 -> 0..255 ease-in-out.
static uint8_t smooth8(uint8_t x) {
    const uint32_t t = x;
    return uint8_t(t * t * (765u - 2u * t) / 65025u);
}

// Smooth 0..255..0 wave over one byte of phase.
static uint8_t wave8(uint8_t phase) { return smooth8(phase < 128 ? phase * 2 : (255 - phase) * 2); }

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

static uint8_t add8(uint8_t a, uint8_t b) { return a + b > 255 ? 255 : a + b; }
static Rgb addRgb(Rgb a, Rgb b) { return Rgb{add8(a.r, b.r), add8(a.g, b.g), add8(a.b, b.b)}; }
static uint8_t max8(uint8_t a, uint8_t b) { return a > b ? a : b; }

// Value noise: a new random target every stepMs, eased between targets.
static uint8_t noise8(uint32_t seed, uint32_t nowMs, uint32_t stepMs) {
    const uint32_t step = nowMs / stepMs;
    const uint8_t frac = smooth8(uint8_t((nowMs % stepMs) * 255 / stepMs));
    return lerp8(hash8(seed + step), hash8(seed + step + 1), frac);
}

Rgb scaleRgb(Rgb c, uint8_t level) {
    return Rgb{uint8_t(c.r * level / 255), uint8_t(c.g * level / 255), uint8_t(c.b * level / 255)};
}

uint8_t idleFade(uint32_t sinceMs, uint32_t fadeMs) {
    if (sinceMs >= fadeMs) return 255;
    return uint8_t(sinceMs * 255 / fadeMs);
}

// Black -> red -> orange -> yellow, with a touch of white at the hottest.
static Rgb heatColor(uint8_t heat) {
    const uint8_t r = heat > 85 ? 255 : heat * 3;
    const uint8_t g = heat > 85 ? (heat > 170 ? 255 : (heat - 85) * 3) : 0;
    const uint8_t b = heat > 200 ? (heat - 200) * 2 : 0;
    return Rgb{r, g, b};
}

// Station 1: flames lick up from both ends; sparks drift above them.
// energy (0..255) makes the flames reach further.
static Rgb fire(uint16_t index, uint16_t count, uint32_t nowMs, uint8_t energy) {
    const uint16_t half = count / 2 ? count / 2 : 1;
    const bool fromEnd = index >= count / 2;
    const uint16_t d = fromEnd ? count - 1 - index : index;
    const uint8_t dist = uint8_t(d >= half ? 255 : uint32_t(d) * 255 / half);
    const uint8_t height = uint8_t(110 + noise8(fromEnd ? 911u : 17u, nowMs, 300) * 90 / 255 + energy * 55 / 255);
    uint8_t heat = dist < height ? 255 - uint32_t(dist) * 255 / height : 0;
    heat = uint8_t(heat * (150 + noise8(uint32_t(index) * 7919u, nowMs, 70) * 105 / 255) / 255);
    if (dist >= height && hash8(uint32_t(index) * 97u + nowMs / 45) > 250) return Rgb{255, 200, 40};
    return heatColor(max8(heat, 25));  // embers never go fully dark
}

// Station 2: three swells of different lengths roll across; foam glints on crests.
static Rgb ocean(uint16_t index, uint8_t pos, uint32_t nowMs, uint8_t energy) {
    static const Rgb DEEP = {0, 15, 90}, SHALLOW = {0, 170, 230}, FOAM = {255, 255, 255};
    const uint8_t level = (wave8(uint8_t(pos + nowMs / 22)) + wave8(uint8_t(pos * 2 - nowMs / 35)) +
                           wave8(uint8_t(pos * 3 + nowMs / 53))) / 3;
    Rgb c = lerpRgb(DEEP, SHALLOW, level);
    const uint8_t crest = 170 - energy / 4;
    if (level > crest && hash8(uint32_t(index) * 53u + nowMs / 60) > 150) {
        c = lerpRgb(c, FOAM, uint8_t((level - crest) * 255 / (255 - crest)));
    }
    return c;
}

// Station 3: green -> teal -> purple curtains drift over a starry night sky.
static Rgb northernLights(uint16_t index, uint8_t pos, uint32_t nowMs, uint8_t energy) {
    static const Rgb NIGHT = {0, 2, 14}, GREEN = {0, 255, 40}, TEAL = {0, 200, 200}, PURPLE = {140, 0, 255};
    const uint8_t hue = wave8(uint8_t(pos + nowMs / 40));
    const Rgb curtain = hue < 128 ? lerpRgb(GREEN, TEAL, hue * 2) : lerpRgb(TEAL, PURPLE, (hue - 128) * 2);
    uint8_t glow = wave8(uint8_t(pos * 2 - nowMs / 27)) * wave8(uint8_t(pos / 2 + nowMs / 61)) / 255;
    glow = add8(glow, energy / 3);
    Rgb c = addRgb(NIGHT, scaleRgb(curtain, glow));
    if (glow < 60 && hash8(uint32_t(index) * 131u + nowMs / 700) > 235) {
        const uint8_t star = wave8(uint8_t(nowMs / 6 + index * 37u));
        c = Rgb{max8(c.r, star), max8(c.g, star), max8(c.b, star)};
    }
    return c;
}

// Station 4: three comets race at different speeds, crackling white near their heads.
static Rgb electric(uint16_t index, uint16_t count, uint32_t nowMs, uint8_t energy) {
    static const Rgb COLORS[3] = {{200, 0, 255}, {0, 220, 255}, {60, 90, 255}};
    static const uint32_t MS_PER_LED[3] = {18, 27, 41};
    static const bool FORWARD[3] = {true, false, true};
    const uint16_t tail = count / 10 > 2 ? count / 10 : 2;
    Rgb c = {0, 0, 12};
    bool nearHead = false;
    for (uint8_t k = 0; k < 3; k++) {
        const uint16_t head = (nowMs / MS_PER_LED[k]) % count;
        const uint16_t behind = FORWARD[k] ? (head + count - index) % count : (index + count - head) % count;
        if (behind >= tail) continue;
        const uint8_t level = uint8_t(255 * (tail - behind) / tail);
        c = addRgb(c, scaleRgb(COLORS[k], uint8_t(level * (190 + energy / 4) / 255)));
        nearHead = nearHead || behind < 2;
    }
    if (nearHead && hash8(uint32_t(index) * 71u + nowMs / 30) > 200 - energy / 2) return Rgb{255, 255, 255};
    return c;
}

static Rgb ambient(uint8_t stationId, uint16_t index, uint16_t count, uint32_t nowMs, uint8_t energy) {
    const uint8_t pos = uint8_t(uint32_t(index) * 256 / count);
    switch (stationId) {
        case 1: return fire(index, count, nowMs, energy);
        case 2: return ocean(index, pos, nowMs, energy);
        case 3: return northernLights(index, pos, nowMs, energy);
        default: return electric(index, count, nowMs, energy);
    }
}

// Invitation envelope (0..255) at `phase` ms into the cycle.
static uint8_t inviteEnergy(uint32_t phase) {
    if (phase < INVITE_SWELL_START_MS) return 0;
    if (phase < INVITE_CHASE_START_MS) {
        return smooth8(uint8_t((phase - INVITE_SWELL_START_MS) * 255 / (INVITE_CHASE_START_MS - INVITE_SWELL_START_MS)));
    }
    if (phase < INVITE_BURST_START_MS) return 255;
    return smooth8(uint8_t(255 - (phase - INVITE_BURST_START_MS) * 255 / (INVITE_PERIOD_MS - INVITE_BURST_START_MS)));
}

static Rgb buttonColor(uint8_t button, uint8_t level) {
    return scaleRgb(scaleRgb(maskColor(1u << button), level), IDLE_MAX_LEVEL);
}

Rgb idlePixel(uint8_t stationId, uint16_t index, uint16_t count, uint32_t nowMs) {
    if (stationId < 1 || stationId > 4) return LED_IDLE;
    if (count == 0) return Rgb{0, 0, 0};

    const uint32_t cycle = nowMs / INVITE_PERIOD_MS;
    const uint32_t phase = nowMs % INVITE_PERIOD_MS;
    const uint8_t energy = inviteEnergy(phase);
    const uint8_t level = uint8_t(IDLE_AMBIENT_LEVEL + (IDLE_MAX_LEVEL - IDLE_AMBIENT_LEVEL) * energy / 255);
    const Rgb base = scaleRgb(ambient(stationId, index, count, nowMs, energy), level);
    const uint16_t along = cycle % 2 ? count - 1 - index : index;  // alternate direction

    if (phase >= INVITE_CHASE_START_MS && phase < INVITE_BURST_START_MS) {
        // Five dots in the button colors, red first, run the length of the strip.
        const int32_t spacing = count / 8 > 2 ? count / 8 : 2;
        const int32_t tail = count / 12 > 2 ? count / 12 : 2;
        const int32_t travel = count + 4 * spacing + tail;
        const int32_t lead = int32_t((phase - INVITE_CHASE_START_MS) * travel / (INVITE_BURST_START_MS - INVITE_CHASE_START_MS));
        for (uint8_t b = 0; b < BUTTON_COUNT; b++) {
            const int32_t behind = lead - b * spacing - along;
            if (behind >= 0 && behind < tail) return buttonColor(b, uint8_t(255 * (tail - behind) / tail));
        }
    } else if (phase >= INVITE_BURST_START_MS) {
        // The strip bursts into five button-colored segments that fade back to ambient.
        const uint8_t button = (uint32_t(along) * BUTTON_COUNT / count + cycle) % BUTTON_COUNT;
        const uint8_t left = uint8_t(255 - (phase - INVITE_BURST_START_MS) * 255 / (INVITE_PERIOD_MS - INVITE_BURST_START_MS));
        return lerpRgb(base, buttonColor(button, 255), uint8_t(left * left / 255));
    }
    return base;
}
