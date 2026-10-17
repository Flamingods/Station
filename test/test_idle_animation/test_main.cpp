#include <unity.h>
#include "IdleAnimation.h"

void setUp() {}
void tearDown() {}

static const uint16_t COUNT = 100;

// Phase offsets inside one invitation cycle (see IdleAnimation.h).
static const uint32_t AMBIENT_AT = 1000;
static const uint32_t CHASE_AT = INVITE_CHASE_START_MS;
static const uint32_t BURST_AT = INVITE_BURST_START_MS;

static bool samePixel(Rgb a, Rgb b) { return a.r == b.r && a.g == b.g && a.b == b.b; }

static bool sameFrame(uint8_t stationA, uint8_t stationB, uint32_t tA, uint32_t tB) {
    for (uint16_t i = 0; i < COUNT; i++) {
        if (!samePixel(idlePixel(stationA, i, COUNT, tA), idlePixel(stationB, i, COUNT, tB))) return false;
    }
    return true;
}

static uint8_t max2(uint8_t a, uint8_t b) { return a > b ? a : b; }
static uint8_t maxChannel(Rgb c) { return max2(c.r, max2(c.g, c.b)); }

static bool inAmbient(uint32_t t) { return t % INVITE_PERIOD_MS < INVITE_SWELL_START_MS; }

// Calls check(pixel) for every pixel of the ambient part of 40 s of animation.
template <typename F>
static void forEachAmbientPixel(uint8_t station, F check) {
    for (uint32_t t = 0; t < 40000; t += 37) {
        if (!inAmbient(t)) continue;
        for (uint16_t i = 0; i < COUNT; i++) check(idlePixel(station, i, COUNT, t));
    }
}

static Rgb buttonColor(uint8_t button) { return scaleRgb(maskColor(1u << button), IDLE_MAX_LEVEL); }

void test_each_station_has_its_own_animation() {
    for (uint8_t a = 1; a <= 4; a++) {
        for (uint8_t b = a + 1; b <= 4; b++) {
            TEST_ASSERT_FALSE_MESSAGE(sameFrame(a, b, AMBIENT_AT, AMBIENT_AT), "two stations share an animation");
        }
    }
}

void test_never_exceeds_peak_level() {
    for (uint8_t s = 1; s <= 4; s++) {
        for (uint32_t t = 0; t < 2 * INVITE_PERIOD_MS; t += 13) {
            for (uint16_t i = 0; i < COUNT; i++) TEST_ASSERT_TRUE(maxChannel(idlePixel(s, i, COUNT, t)) <= IDLE_MAX_LEVEL);
        }
    }
}

void test_ambient_stays_dim_and_never_dark() {
    for (uint8_t s = 1; s <= 4; s++) {
        forEachAmbientPixel(s, [](Rgb c) {
            TEST_ASSERT_TRUE(maxChannel(c) <= IDLE_AMBIENT_LEVEL);
            TEST_ASSERT_TRUE(maxChannel(c) > 0);  // a dark strip looks switched off
        });
    }
}

void test_station1_fire_colors() {
    forEachAmbientPixel(1, [](Rgb c) {
        TEST_ASSERT_TRUE(c.g <= c.r);
        TEST_ASSERT_TRUE(c.b <= c.g);
    });
}

void test_station2_ocean_colors() {
    forEachAmbientPixel(2, [](Rgb c) { TEST_ASSERT_TRUE(c.b >= c.g && c.b >= c.r); });
}

void test_station3_northern_lights_colors() {
    forEachAmbientPixel(3, [](Rgb c) { TEST_ASSERT_TRUE(c.r <= max2(c.g, c.b)); });
}

void test_station4_electric_colors() {
    forEachAmbientPixel(4, [](Rgb c) { TEST_ASSERT_TRUE(c.b >= max2(c.r, c.g)); });
}

void test_every_station_animates() {
    for (uint8_t s = 1; s <= 4; s++) {
        TEST_ASSERT_FALSE_MESSAGE(sameFrame(s, s, 1000, 3000), "idle animation is static");
    }
}

void test_swell_brightens_past_ambient() {
    for (uint8_t s = 1; s <= 4; s++) {
        uint8_t brightest = 0;
        for (uint16_t i = 0; i < COUNT; i++) {
            brightest = max2(brightest, maxChannel(idlePixel(s, i, COUNT, CHASE_AT - 1)));
        }
        TEST_ASSERT_TRUE(brightest > IDLE_AMBIENT_LEVEL);
    }
}

void test_chase_shows_all_five_button_colors() {
    for (uint8_t s = 1; s <= 4; s++) {
        bool seen[5] = {};
        for (uint32_t t = CHASE_AT; t < BURST_AT; t += 5) {
            for (uint16_t i = 0; i < COUNT; i++) {
                const Rgb c = idlePixel(s, i, COUNT, t);
                for (uint8_t b = 0; b < 5; b++) seen[b] = seen[b] || samePixel(c, buttonColor(b));
            }
        }
        for (uint8_t b = 0; b < 5; b++) TEST_ASSERT_TRUE_MESSAGE(seen[b], "chase misses a button color");
    }
}

void test_chase_alternates_direction() {
    const uint32_t t = CHASE_AT + 500;
    for (uint8_t s = 1; s <= 4; s++) {
        // The green dot's head (no theme ever shows pure green) sits near the start of
        // the strip on even cycles and mirrored near the end on odd ones.
        const Rgb lead = buttonColor(1);
        int16_t even = -1, odd = -1;
        for (uint16_t i = 0; i < COUNT; i++) {
            if (samePixel(idlePixel(s, i, COUNT, t), lead)) even = i;
            if (samePixel(idlePixel(s, i, COUNT, t + INVITE_PERIOD_MS), lead)) odd = i;
        }
        TEST_ASSERT_TRUE(even >= 0 && odd >= 0);
        TEST_ASSERT_EQUAL_INT16(COUNT - 1 - even, odd);
    }
}

void test_burst_fills_strip_with_button_colors() {
    for (uint8_t s = 1; s <= 4; s++) {
        bool seen[5] = {};
        for (uint16_t i = 0; i < COUNT; i++) {
            const Rgb c = idlePixel(s, i, COUNT, BURST_AT);
            bool isButton = false;
            for (uint8_t b = 0; b < 5; b++) {
                if (samePixel(c, buttonColor(b))) seen[b] = isButton = true;
            }
            TEST_ASSERT_TRUE_MESSAGE(isButton, "burst pixel is not a button color");
        }
        for (uint8_t b = 0; b < 5; b++) TEST_ASSERT_TRUE(seen[b]);
    }
}

void test_burst_settles_back_to_ambient() {
    for (uint8_t s = 1; s <= 4; s++) {
        for (uint16_t i = 0; i < COUNT; i++) {
            TEST_ASSERT_TRUE(maxChannel(idlePixel(s, i, COUNT, INVITE_PERIOD_MS - 1)) <= IDLE_AMBIENT_LEVEL + 2);
        }
    }
}

void test_keeps_moving_across_millis_wraparound() {
    for (uint8_t s = 1; s <= 4; s++) {
        TEST_ASSERT_FALSE(sameFrame(s, s, 0xFFFFFF00u, 0x00000100u));
    }
}

void test_unknown_station_falls_back_to_glow() {
    TEST_ASSERT_TRUE(samePixel(LED_IDLE, idlePixel(0, 3, COUNT, 1234)));
    TEST_ASSERT_TRUE(samePixel(LED_IDLE, idlePixel(9, 3, COUNT, 1234)));
}

void test_short_and_empty_strips_are_safe() {
    for (uint8_t s = 1; s <= 4; s++) {
        for (uint32_t t = 0; t < INVITE_PERIOD_MS; t += 50) {
            TEST_ASSERT_TRUE(maxChannel(idlePixel(s, 0, 0, t)) <= IDLE_MAX_LEVEL);
            for (uint16_t i = 0; i < 3; i++) TEST_ASSERT_TRUE(maxChannel(idlePixel(s, i, 3, t)) <= IDLE_MAX_LEVEL);
        }
    }
}

void test_fade_in_after_release() {
    TEST_ASSERT_EQUAL_UINT8(0, idleFade(0, 1000));
    TEST_ASSERT_EQUAL_UINT8(127, idleFade(500, 1000));
    TEST_ASSERT_EQUAL_UINT8(255, idleFade(1000, 1000));
    TEST_ASSERT_EQUAL_UINT8(255, idleFade(5000, 1000));
    TEST_ASSERT_EQUAL_UINT8(255, idleFade(0, 0));
}

void test_scale_rgb() {
    Rgb c = scaleRgb(Rgb{255, 100, 0}, 128);
    TEST_ASSERT_EQUAL_UINT8(128, c.r);
    TEST_ASSERT_EQUAL_UINT8(50, c.g);
    TEST_ASSERT_EQUAL_UINT8(0, c.b);
    TEST_ASSERT_TRUE(samePixel(Rgb{0, 0, 0}, scaleRgb(Rgb{255, 255, 255}, 0)));
}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_each_station_has_its_own_animation);
    RUN_TEST(test_never_exceeds_peak_level);
    RUN_TEST(test_ambient_stays_dim_and_never_dark);
    RUN_TEST(test_station1_fire_colors);
    RUN_TEST(test_station2_ocean_colors);
    RUN_TEST(test_station3_northern_lights_colors);
    RUN_TEST(test_station4_electric_colors);
    RUN_TEST(test_every_station_animates);
    RUN_TEST(test_swell_brightens_past_ambient);
    RUN_TEST(test_chase_shows_all_five_button_colors);
    RUN_TEST(test_chase_alternates_direction);
    RUN_TEST(test_burst_fills_strip_with_button_colors);
    RUN_TEST(test_burst_settles_back_to_ambient);
    RUN_TEST(test_keeps_moving_across_millis_wraparound);
    RUN_TEST(test_unknown_station_falls_back_to_glow);
    RUN_TEST(test_short_and_empty_strips_are_safe);
    RUN_TEST(test_fade_in_after_release);
    RUN_TEST(test_scale_rgb);
    return UNITY_END();
}
