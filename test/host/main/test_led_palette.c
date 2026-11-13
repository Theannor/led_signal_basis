#include <string.h>
#include "unity.h"
#include "led_palette.h"
#include "led_twinkle.h"

static void assert_color(uint8_t r, uint8_t g, uint8_t b, led_color_t actual)
{
    TEST_ASSERT_EQUAL_UINT8(r, actual.r);
    TEST_ASSERT_EQUAL_UINT8(g, actual.g);
    TEST_ASSERT_EQUAL_UINT8(b, actual.b);
}

static void test_from_json_object(void)
{
    led_palette_t p;
    TEST_ASSERT_TRUE(led_palette_from_json("{\"colors\":[\"#ff0000\",\"00FF80\"]}", &p));
    TEST_ASSERT_EQUAL_size_t(2, p.count);
    assert_color(255, 0, 0, p.colors[0]);
    assert_color(0, 255, 128, p.colors[1]);
}

static void test_from_json_bare_array_and_whitespace(void)
{
    led_palette_t p;
    TEST_ASSERT_TRUE(led_palette_from_json(" [ \"#0a0b0c\" ,\n\"#ffffff\" ] ", &p));
    TEST_ASSERT_EQUAL_size_t(2, p.count);
    assert_color(10, 11, 12, p.colors[0]);
    TEST_ASSERT_TRUE(led_palette_from_json("{ \"colors\" : [ \"#123456\" ] }\n", &p));
    TEST_ASSERT_EQUAL_size_t(1, p.count);
    assert_color(0x12, 0x34, 0x56, p.colors[0]);
}

static void test_from_json_empty_list(void)
{
    led_palette_t p = {.count = 3};
    TEST_ASSERT_TRUE(led_palette_from_json("{\"colors\":[]}", &p));
    TEST_ASSERT_EQUAL_size_t(0, p.count);
}

static void test_from_json_rejects_bad_input(void)
{
    static const char *bad[] = {
        "",
        "{}",
        "{\"farben\":[]}",
        "{\"colors\":[\"#ff00\"]}",
        "{\"colors\":[\"#ff00zz\"]}",
        "{\"colors\":[\"#ff00000\"]}",
        "{\"colors\":[\"#ff0000\",]}",
        "{\"colors\":[\"#ff0000\"]",
        "{\"colors\":[\"#ff0000\"]} x",
        "[\"#ff0000\" \"#00ff00\"]",
        "[#ff0000]",
        "[\"#ff",
    };
    for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
        led_palette_t p = {.count = 7};
        TEST_ASSERT_FALSE_MESSAGE(led_palette_from_json(bad[i], &p), bad[i]);
        TEST_ASSERT_EQUAL_size_t(7, p.count);
    }
    led_palette_t p;
    TEST_ASSERT_FALSE(led_palette_from_json(NULL, &p));
    TEST_ASSERT_FALSE(led_palette_from_json("[]", NULL));
}

static void test_from_json_limits_count(void)
{
    char json[400] = "[";
    for (int i = 0; i < LED_PALETTE_MAX; i++) {
        strcat(json, i > 0 ? ",\"#010203\"" : "\"#010203\"");
    }
    strcat(json, "]");
    led_palette_t p;
    TEST_ASSERT_TRUE(led_palette_from_json(json, &p));
    TEST_ASSERT_EQUAL_size_t(LED_PALETTE_MAX, p.count);
    // Eine Farbe mehr ist zu viel
    json[strlen(json) - 1] = '\0';
    strcat(json, ",\"#010203\"]");
    TEST_ASSERT_FALSE(led_palette_from_json(json, &p));
}

static void test_to_json(void)
{
    led_palette_t p = {.colors = {{.r = 255}, {.g = 0xab, .b = 1}}, .count = 2};
    char buf[64];
    int n = led_palette_to_json(&p, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[\"#ff0000\",\"#00ab01\"]}", buf);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
    p.count = 0;
    led_palette_to_json(&p, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[]}", buf);
}

static void test_to_json_round_trip_and_errors(void)
{
    led_palette_t p = {.count = LED_PALETTE_MAX}, back;
    for (size_t i = 0; i < LED_PALETTE_MAX; i++) {
        p.colors[i] = (led_color_t){.r = (uint8_t)i, .g = (uint8_t)(i * 7), .b = 200};
    }
    char buf[256];
    TEST_ASSERT_GREATER_THAN(0, led_palette_to_json(&p, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(led_palette_from_json(buf, &back));
    TEST_ASSERT_EQUAL_MEMORY(&p, &back, sizeof(p));

    char small[20];
    TEST_ASSERT_EQUAL_INT(-1, led_palette_to_json(&p, small, sizeof(small)));
    TEST_ASSERT_EQUAL_INT(-1, led_palette_to_json(NULL, buf, sizeof(buf)));
    p.count = LED_PALETTE_MAX + 1;
    TEST_ASSERT_EQUAL_INT(-1, led_palette_to_json(&p, buf, sizeof(buf)));
}

// Feste "Zufallsfolge" für die Funkel-Tests
static const uint32_t *s_seq;
static size_t s_pos;
static uint32_t fake_rand(void)
{
    return s_seq[s_pos++];
}

static void test_twinkle_picks_from_palette(void)
{
    // Index 5 % 3 = 2 -> Weiß; Fadezeit 1000 + 10
    static const uint32_t seq[] = {5, 10};
    s_seq = seq;
    s_pos = 0;
    const led_color_t colors[] = {{.r = 255}, {.g = 255}, {.r = 255, .g = 255, .b = 255}};
    led_twinkle_cfg_t cfg = {
        .rand_fn = fake_rand, .min_fade_ms = 1000, .max_fade_ms = 4000,
        .palette = colors, .palette_count = 3,
    };
    led_twinkle_t t;
    led_twinkle_start(&t, &cfg);
    assert_color(255, 255, 255, t.color);
    TEST_ASSERT_EQUAL_UINT32(1010, t.fade_ms);
}

static void test_twinkle_empty_palette_uses_hue(void)
{
    static const uint32_t seq[] = {240, 0};
    s_seq = seq;
    s_pos = 0;
    const led_color_t colors[] = {{.r = 255}};
    led_twinkle_cfg_t cfg = {
        .rand_fn = fake_rand, .min_fade_ms = 1000, .max_fade_ms = 4000,
        .palette = colors, .palette_count = 0,
    };
    led_twinkle_t t;
    led_twinkle_start(&t, &cfg);
    assert_color(0, 0, 255, t.color);
}

static void test_from_bytes(void)
{
    const uint8_t data[] = {0xff, 0x00, 0x00, 0x00, 0xff, 0x80};
    led_palette_t p;
    TEST_ASSERT_TRUE(led_palette_from_bytes(data, sizeof(data), &p));
    TEST_ASSERT_EQUAL_size_t(2, p.count);
    assert_color(255, 0, 0, p.colors[0]);
    assert_color(0, 255, 128, p.colors[1]);
}

static void test_from_bytes_empty(void)
{
    led_palette_t p = {.count = 5};
    TEST_ASSERT_TRUE(led_palette_from_bytes(NULL, 0, &p));
    TEST_ASSERT_EQUAL_size_t(0, p.count);
}

static void test_from_bytes_rejects_bad_length(void)
{
    uint8_t data[3 * LED_PALETTE_MAX + 3] = {0};
    led_palette_t p = {.count = 1, .colors = {{.r = 7}}};
    TEST_ASSERT_FALSE(led_palette_from_bytes(data, 2, &p));
    TEST_ASSERT_FALSE(led_palette_from_bytes(data, 4, &p));
    TEST_ASSERT_FALSE(led_palette_from_bytes(data, sizeof(data), &p));
    TEST_ASSERT_FALSE(led_palette_from_bytes(NULL, 3, &p));
    TEST_ASSERT_FALSE(led_palette_from_bytes(data, 3, NULL));
    // out bleibt bei Fehler unverändert
    TEST_ASSERT_EQUAL_size_t(1, p.count);
    TEST_ASSERT_EQUAL_UINT8(7, p.colors[0].r);
    // Genau LED_PALETTE_MAX Farben sind erlaubt
    TEST_ASSERT_TRUE(led_palette_from_bytes(data, 3 * LED_PALETTE_MAX, &p));
    TEST_ASSERT_EQUAL_size_t(LED_PALETTE_MAX, p.count);
}

static void test_to_bytes_round_trip_and_errors(void)
{
    led_palette_t p = {.count = 2, .colors = {{.r = 1, .g = 2, .b = 3}, {.r = 4, .g = 5, .b = 6}}};
    uint8_t buf[3 * LED_PALETTE_MAX];
    TEST_ASSERT_EQUAL_INT(6, led_palette_to_bytes(&p, buf, sizeof(buf)));
    const uint8_t expected[] = {1, 2, 3, 4, 5, 6};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, 6);

    led_palette_t back;
    TEST_ASSERT_TRUE(led_palette_from_bytes(buf, 6, &back));
    TEST_ASSERT_EQUAL_size_t(2, back.count);
    assert_color(4, 5, 6, back.colors[1]);

    TEST_ASSERT_EQUAL_INT(-1, led_palette_to_bytes(&p, buf, 5));
    TEST_ASSERT_EQUAL_INT(-1, led_palette_to_bytes(NULL, buf, sizeof(buf)));
    led_palette_t empty = {0};
    TEST_ASSERT_EQUAL_INT(0, led_palette_to_bytes(&empty, buf, 0));
    p.count = LED_PALETTE_MAX + 1;
    TEST_ASSERT_EQUAL_INT(-1, led_palette_to_bytes(&p, buf, sizeof(buf)));
}

static void test_is_off(void)
{
    led_palette_t p = {.count = 1};
    TEST_ASSERT_TRUE(led_palette_is_off(&p));
    // Mehrere schwarze Farben gelten ebenfalls als aus
    p.count = 3;
    TEST_ASSERT_TRUE(led_palette_is_off(&p));
    // Eine einzige nicht-schwarze Farbe reicht, auch nur ein Kanal
    p.colors[2].b = 1;
    TEST_ASSERT_FALSE(led_palette_is_off(&p));
}

static void test_is_off_edge_cases(void)
{
    // Leere Palette bedeutet zufällige Farbtöne, nicht aus
    led_palette_t empty = {0};
    TEST_ASSERT_FALSE(led_palette_is_off(&empty));
    TEST_ASSERT_FALSE(led_palette_is_off(NULL));
    led_palette_t too_many = {.count = LED_PALETTE_MAX + 1};
    TEST_ASSERT_FALSE(led_palette_is_off(&too_many));
    led_palette_t json;
    TEST_ASSERT_TRUE(led_palette_from_json("{\"colors\":[\"#000000\"]}", &json));
    TEST_ASSERT_TRUE(led_palette_is_off(&json));
}

// Wird von test_main.c aufgerufen
void run_led_palette_tests(void)
{
    RUN_TEST(test_from_json_object);
    RUN_TEST(test_from_json_bare_array_and_whitespace);
    RUN_TEST(test_from_json_empty_list);
    RUN_TEST(test_from_json_rejects_bad_input);
    RUN_TEST(test_from_json_limits_count);
    RUN_TEST(test_to_json);
    RUN_TEST(test_to_json_round_trip_and_errors);
    RUN_TEST(test_twinkle_picks_from_palette);
    RUN_TEST(test_twinkle_empty_palette_uses_hue);
    RUN_TEST(test_from_bytes);
    RUN_TEST(test_from_bytes_empty);
    RUN_TEST(test_from_bytes_rejects_bad_length);
    RUN_TEST(test_to_bytes_round_trip_and_errors);
    RUN_TEST(test_is_off);
    RUN_TEST(test_is_off_edge_cases);
}
