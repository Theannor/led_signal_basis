#include <stdint.h>
#include "unity.h"
#include "led_twinkle.h"

// Vorgegebene "Zufallsfolge" für reproduzierbare Tests
static const uint32_t *s_seq;
static size_t s_pos;
static uint32_t fake_rand(void)
{
    return s_seq[s_pos++];
}

static void set_seq(const uint32_t *seq)
{
    s_seq = seq;
    s_pos = 0;
}

static const led_twinkle_cfg_t CFG = {
    .rand_fn = fake_rand,
    .min_fade_ms = 1000,
    .max_fade_ms = 4000,
};

static void assert_color(uint8_t r, uint8_t g, uint8_t b, led_color_t actual)
{
    TEST_ASSERT_EQUAL_UINT8(r, actual.r);
    TEST_ASSERT_EQUAL_UINT8(g, actual.g);
    TEST_ASSERT_EQUAL_UINT8(b, actual.b);
}

// LED mit fester Farbe und Fadezeit, ohne Zufall
static led_twinkle_t red_led(uint32_t fade_ms, uint32_t elapsed_ms)
{
    led_twinkle_t t = {.color = {.r = 255}, .fade_ms = fade_ms, .elapsed_ms = elapsed_ms};
    return t;
}

static void test_start_uses_random_hue_and_fade(void)
{
    // Farbton 120 (Grün), Fadezeit 1000 + 500 % 3001 = 1500
    static const uint32_t seq[] = {120, 500};
    set_seq(seq);
    led_twinkle_t t = {.elapsed_ms = 99};
    led_twinkle_start(&t, &CFG);
    assert_color(0, 255, 0, t.color);
    TEST_ASSERT_EQUAL_UINT32(1500, t.fade_ms);
    TEST_ASSERT_EQUAL_UINT32(0, t.elapsed_ms);
}

static void test_start_keeps_fade_in_range(void)
{
    static const uint32_t seq[] = {0, 0, 0, 3000, 0, UINT32_MAX};
    set_seq(seq);
    led_twinkle_t t;
    led_twinkle_start(&t, &CFG);
    TEST_ASSERT_EQUAL_UINT32(1000, t.fade_ms);
    led_twinkle_start(&t, &CFG);
    TEST_ASSERT_EQUAL_UINT32(4000, t.fade_ms);
    led_twinkle_start(&t, &CFG);
    TEST_ASSERT_UINT32_WITHIN(1500, 2500, t.fade_ms);
}

static void test_start_handles_odd_ranges(void)
{
    // max < min -> min; 0..0 -> mindestens 1 ms
    static const uint32_t seq[] = {0, 77, 0, 77};
    set_seq(seq);
    led_twinkle_t t;
    led_twinkle_cfg_t swapped = {.rand_fn = fake_rand, .min_fade_ms = 2000, .max_fade_ms = 1000};
    led_twinkle_start(&t, &swapped);
    TEST_ASSERT_EQUAL_UINT32(2000, t.fade_ms);
    led_twinkle_cfg_t zero = {.rand_fn = fake_rand, .min_fade_ms = 0, .max_fade_ms = 0};
    led_twinkle_start(&t, &zero);
    TEST_ASSERT_EQUAL_UINT32(1, t.fade_ms);
}

static void test_color_is_black_at_start_and_end(void)
{
    led_twinkle_t t = red_led(1000, 0);
    assert_color(0, 0, 0, led_twinkle_color(&t, 255));
    t.elapsed_ms = 2000;
    assert_color(0, 0, 0, led_twinkle_color(&t, 255));
}

static void test_color_is_full_at_peak(void)
{
    led_twinkle_t t = red_led(1000, 1000);
    assert_color(255, 0, 0, led_twinkle_color(&t, 255));
    assert_color(25, 0, 0, led_twinkle_color(&t, 25));
}

static void test_color_fades_in_and_out_symmetrically(void)
{
    // Halbe Strecke: 500 * 255 / 1000 = 127,5 -> 128
    led_twinkle_t in = red_led(1000, 500);
    led_twinkle_t out = red_led(1000, 1500);
    assert_color(128, 0, 0, led_twinkle_color(&in, 255));
    assert_color(128, 0, 0, led_twinkle_color(&out, 255));
    in.elapsed_ms = 250;
    out.elapsed_ms = 1750;
    led_twinkle_t later = red_led(1000, 750);
    TEST_ASSERT_LESS_THAN_UINT8(led_twinkle_color(&later, 255).r, led_twinkle_color(&in, 255).r);
    TEST_ASSERT_EQUAL_UINT8(led_twinkle_color(&in, 255).r, led_twinkle_color(&out, 255).r);
}

static void test_update_advances_and_writes_colors(void)
{
    led_twinkle_t leds[2] = {red_led(1000, 0), red_led(2000, 0)};
    led_color_t out[2];
    led_twinkle_update(leds, 2, 1000, &CFG, 255, out);
    TEST_ASSERT_EQUAL_UINT32(1000, leds[0].elapsed_ms);
    assert_color(255, 0, 0, out[0]);
    assert_color(128, 0, 0, out[1]);
}

static void test_update_restarts_finished_cycle(void)
{
    // Zyklus endet bei 2000 ms -> neue Farbe Blau (240), Fadezeit 1000 + 2 = 1002
    static const uint32_t seq[] = {240, 2};
    set_seq(seq);
    led_twinkle_t leds[1] = {red_led(1000, 1950)};
    led_color_t out[1];
    led_twinkle_update(leds, 1, 50, &CFG, 255, out);
    assert_color(0, 0, 255, leds[0].color);
    TEST_ASSERT_EQUAL_UINT32(1002, leds[0].fade_ms);
    TEST_ASSERT_EQUAL_UINT32(0, leds[0].elapsed_ms);
    assert_color(0, 0, 0, out[0]);
}

static void test_update_ignores_empty_input(void)
{
    led_twinkle_t t = red_led(1000, 0);
    led_color_t c = {.r = 7, .g = 7, .b = 7};
    led_twinkle_update(NULL, 1, 50, &CFG, 255, &c);
    led_twinkle_update(&t, 1, 50, &CFG, 255, NULL);
    led_twinkle_update(&t, 0, 50, &CFG, 255, &c);
    assert_color(7, 7, 7, c);
    TEST_ASSERT_EQUAL_UINT32(0, t.elapsed_ms);
}

// Wird von test_main.c aufgerufen
void run_led_twinkle_tests(void)
{
    RUN_TEST(test_start_uses_random_hue_and_fade);
    RUN_TEST(test_start_keeps_fade_in_range);
    RUN_TEST(test_start_handles_odd_ranges);
    RUN_TEST(test_color_is_black_at_start_and_end);
    RUN_TEST(test_color_is_full_at_peak);
    RUN_TEST(test_color_fades_in_and_out_symmetrically);
    RUN_TEST(test_update_advances_and_writes_colors);
    RUN_TEST(test_update_restarts_finished_cycle);
    RUN_TEST(test_update_ignores_empty_input);
}
