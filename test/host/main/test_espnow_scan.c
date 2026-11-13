#include "unity.h"
#include "espnow_scan.h"

static espnow_scan_t make(uint8_t max_channel, uint8_t tries, uint8_t misses)
{
    espnow_scan_t s = {
        .max_channel = max_channel,
        .tries_per_channel = tries,
        .max_misses = misses,
        .retry_ms = 60,
        .keepalive_ms = 30000,
    };
    espnow_scan_init(&s);
    return s;
}

static void test_init_starts_searching_on_channel_1(void)
{
    espnow_scan_t s = make(13, 2, 3);
    TEST_ASSERT_EQUAL(ESPNOW_SCAN_SEARCHING, s.state);
    TEST_ASSERT_EQUAL_UINT8(1, s.channel);
    TEST_ASSERT_EQUAL_UINT32(60, espnow_scan_wait_ms(&s));

    // Fehlende Einstellungen werden ergänzt
    espnow_scan_t z = make(0, 0, 0);
    TEST_ASSERT_EQUAL_UINT8(13, z.max_channel);
    TEST_ASSERT_EQUAL_UINT8(1, z.tries_per_channel);
    TEST_ASSERT_EQUAL_UINT8(1, z.max_misses);
}

static void test_search_tries_each_channel_and_wraps(void)
{
    espnow_scan_t s = make(3, 2, 3);
    TEST_ASSERT_FALSE(espnow_scan_result(&s, false));  // 1. Versuch Kanal 1
    TEST_ASSERT_EQUAL_UINT8(1, s.channel);
    TEST_ASSERT_TRUE(espnow_scan_result(&s, false));   // 2. Versuch -> Kanal 2
    TEST_ASSERT_EQUAL_UINT8(2, s.channel);
    espnow_scan_result(&s, false);
    espnow_scan_result(&s, false);
    TEST_ASSERT_EQUAL_UINT8(3, s.channel);
    espnow_scan_result(&s, false);
    TEST_ASSERT_TRUE(espnow_scan_result(&s, false));   // nach dem letzten wieder Kanal 1
    TEST_ASSERT_EQUAL_UINT8(1, s.channel);
    TEST_ASSERT_EQUAL(ESPNOW_SCAN_SEARCHING, s.state);
}

static void test_ack_connects_and_keeps_channel(void)
{
    espnow_scan_t s = make(13, 1, 3);
    espnow_scan_result(&s, false);
    espnow_scan_result(&s, false);
    TEST_ASSERT_EQUAL_UINT8(3, s.channel);
    TEST_ASSERT_FALSE(espnow_scan_result(&s, true));
    TEST_ASSERT_EQUAL(ESPNOW_SCAN_CONNECTED, s.state);
    TEST_ASSERT_EQUAL_UINT8(3, s.channel);
    TEST_ASSERT_EQUAL_UINT32(30000, espnow_scan_wait_ms(&s));
}

static void test_misses_lead_to_new_search(void)
{
    espnow_scan_t s = make(13, 1, 3);
    espnow_scan_result(&s, false);   // Kanal 2
    espnow_scan_result(&s, true);    // gefunden
    // Ein Fehlschlag: schneller nachfragen, aber noch verbunden
    TEST_ASSERT_FALSE(espnow_scan_result(&s, false));
    TEST_ASSERT_EQUAL(ESPNOW_SCAN_CONNECTED, s.state);
    TEST_ASSERT_EQUAL_UINT32(60, espnow_scan_wait_ms(&s));
    // Bestätigung setzt den Zähler zurück
    espnow_scan_result(&s, true);
    TEST_ASSERT_EQUAL_UINT32(30000, espnow_scan_wait_ms(&s));
    espnow_scan_result(&s, false);
    espnow_scan_result(&s, false);
    TEST_ASSERT_EQUAL(ESPNOW_SCAN_CONNECTED, s.state);
    TEST_ASSERT_FALSE(espnow_scan_result(&s, false));  // 3. in Folge
    TEST_ASSERT_EQUAL(ESPNOW_SCAN_SEARCHING, s.state);
    TEST_ASSERT_EQUAL_UINT8(2, s.channel);             // Suche beginnt beim alten Kanal
    TEST_ASSERT_TRUE(espnow_scan_result(&s, false));
    TEST_ASSERT_EQUAL_UINT8(3, s.channel);
}

static void test_null_is_safe(void)
{
    espnow_scan_init(NULL);
    TEST_ASSERT_FALSE(espnow_scan_result(NULL, true));
    TEST_ASSERT_EQUAL_UINT32(0, espnow_scan_wait_ms(NULL));
}

// Wird von test_main.c aufgerufen
void run_espnow_scan_tests(void)
{
    RUN_TEST(test_init_starts_searching_on_channel_1);
    RUN_TEST(test_search_tries_each_channel_and_wraps);
    RUN_TEST(test_ack_connects_and_keeps_channel);
    RUN_TEST(test_misses_lead_to_new_search);
    RUN_TEST(test_null_is_safe);
}
