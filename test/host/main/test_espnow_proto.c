#include <string.h>
#include "unity.h"
#include "espnow_proto.h"
#include "led_palette.h"

static led_palette_t two_colors(void)
{
    led_palette_t p = {.count = 2};
    p.colors[0] = (led_color_t){.r = 255, .g = 0, .b = 0};
    p.colors[1] = (led_color_t){.r = 0, .g = 128, .b = 255};
    return p;
}

static void test_hash_depends_on_content(void)
{
    led_palette_t a = two_colors();
    led_palette_t b = two_colors();
    TEST_ASSERT_EQUAL_HEX32(led_palette_hash(&a), led_palette_hash(&b));
    b.colors[1].b = 254;
    TEST_ASSERT_NOT_EQUAL(led_palette_hash(&a), led_palette_hash(&b));
    // Leer und Anzahl zählen mit; Reste hinter count nicht
    led_palette_t empty = {0};
    led_palette_t one = a;
    one.count = 1;
    TEST_ASSERT_NOT_EQUAL(led_palette_hash(&empty), led_palette_hash(&one));
    one.colors[1].r = 7;
    led_palette_t one_b = a;
    one_b.count = 1;
    TEST_ASSERT_EQUAL_HEX32(led_palette_hash(&one_b), led_palette_hash(&one));
    // Bekannter FNV-1a-Wert der leeren Palette (ein Byte 0)
    TEST_ASSERT_EQUAL_HEX32(0x050c5d1f, led_palette_hash(&empty));
}

static void test_hash_of_invalid_palette_is_zero(void)
{
    led_palette_t p = {.count = LED_PALETTE_MAX + 1};
    TEST_ASSERT_EQUAL_HEX32(0, led_palette_hash(&p));
    TEST_ASSERT_EQUAL_HEX32(0, led_palette_hash(NULL));
}

static void test_hello_roundtrip(void)
{
    uint8_t buf[ESPNOW_PROTO_MAX_LEN];
    int len = espnow_proto_encode_hello(0x12345678, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(ESPNOW_PROTO_HEADER_LEN, len);
    const uint8_t expected[] = {'L', 'S', 1, ESPNOW_MSG_HELLO, 0x78, 0x56, 0x34, 0x12};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(expected, buf, sizeof(expected));
    espnow_msg_t msg;
    TEST_ASSERT_TRUE(espnow_proto_decode(buf, (size_t)len, &msg));
    TEST_ASSERT_EQUAL(ESPNOW_MSG_HELLO, msg.type);
    TEST_ASSERT_EQUAL_HEX32(0x12345678, msg.hash);
    TEST_ASSERT_EQUAL_INT(-1, espnow_proto_encode_hello(1, buf, ESPNOW_PROTO_HEADER_LEN - 1));
}

static void test_palette_roundtrip(void)
{
    led_palette_t p = two_colors();
    uint8_t buf[ESPNOW_PROTO_MAX_LEN];
    int len = espnow_proto_encode_palette(&p, buf, sizeof(buf));
    TEST_ASSERT_EQUAL_INT(ESPNOW_PROTO_HEADER_LEN + 6, len);
    espnow_msg_t msg;
    TEST_ASSERT_TRUE(espnow_proto_decode(buf, (size_t)len, &msg));
    TEST_ASSERT_EQUAL(ESPNOW_MSG_PALETTE, msg.type);
    TEST_ASSERT_EQUAL_HEX32(led_palette_hash(&p), msg.hash);
    TEST_ASSERT_EQUAL_size_t(2, msg.palette.count);
    TEST_ASSERT_EQUAL_UINT8(128, msg.palette.colors[1].g);

    // Leere Palette und volle Palette
    led_palette_t empty = {0};
    TEST_ASSERT_EQUAL_INT(ESPNOW_PROTO_HEADER_LEN, espnow_proto_encode_palette(&empty, buf, sizeof(buf)));
    TEST_ASSERT_TRUE(espnow_proto_decode(buf, ESPNOW_PROTO_HEADER_LEN, &msg));
    TEST_ASSERT_EQUAL_size_t(0, msg.palette.count);
    led_palette_t full = {.count = LED_PALETTE_MAX};
    TEST_ASSERT_EQUAL_INT(ESPNOW_PROTO_MAX_LEN, espnow_proto_encode_palette(&full, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, espnow_proto_encode_palette(&full, buf, sizeof(buf) - 1));
    TEST_ASSERT_LESS_OR_EQUAL(250, ESPNOW_PROTO_MAX_LEN);  // passt in ein ESP-NOW-v1-Paket
}

static void test_decode_rejects_bad_packets(void)
{
    led_palette_t p = two_colors();
    uint8_t buf[ESPNOW_PROTO_MAX_LEN];
    int len = espnow_proto_encode_palette(&p, buf, sizeof(buf));
    espnow_msg_t msg = {.hash = 42};

    uint8_t bad[ESPNOW_PROTO_MAX_LEN];
    memcpy(bad, buf, (size_t)len);
    bad[0] = 'X';
    TEST_ASSERT_FALSE(espnow_proto_decode(bad, (size_t)len, &msg));
    memcpy(bad, buf, (size_t)len);
    bad[2] = 2;  // Version
    TEST_ASSERT_FALSE(espnow_proto_decode(bad, (size_t)len, &msg));
    memcpy(bad, buf, (size_t)len);
    bad[3] = 9;  // Typ
    TEST_ASSERT_FALSE(espnow_proto_decode(bad, (size_t)len, &msg));
    memcpy(bad, buf, (size_t)len);
    bad[len - 1] ^= 1;  // Farbe passt nicht zur Prüfsumme
    TEST_ASSERT_FALSE(espnow_proto_decode(bad, (size_t)len, &msg));
    TEST_ASSERT_FALSE(espnow_proto_decode(buf, (size_t)len - 1, &msg));  // keine ganze Farbe
    TEST_ASSERT_FALSE(espnow_proto_decode(buf, 5, &msg));
    TEST_ASSERT_FALSE(espnow_proto_decode(NULL, 8, &msg));

    // HELLO mit Anhang ist ungültig
    espnow_proto_encode_hello(1, bad, sizeof(bad));
    TEST_ASSERT_FALSE(espnow_proto_decode(bad, ESPNOW_PROTO_HEADER_LEN + 1, &msg));
    TEST_ASSERT_EQUAL_HEX32(42, msg.hash);
}

// Wird von test_main.c aufgerufen
void run_espnow_proto_tests(void)
{
    RUN_TEST(test_hash_depends_on_content);
    RUN_TEST(test_hash_of_invalid_palette_is_zero);
    RUN_TEST(test_hello_roundtrip);
    RUN_TEST(test_palette_roundtrip);
    RUN_TEST(test_decode_rejects_bad_packets);
}
