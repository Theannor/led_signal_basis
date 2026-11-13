#include <string.h>
#include "unity.h"
#include "net_config.h"

// Gültige Grundkonfiguration für die Tests
static net_config_t valid_config(void)
{
    net_config_t cfg = {0};
    strcpy(cfg.ssid, "MeinWLAN");
    strcpy(cfg.password, "geheim123");
    strcpy(cfg.hostname, "led-signal-basis");
    return cfg;
}

static void test_hostname_valid(void)
{
    TEST_ASSERT_TRUE(net_config_hostname_is_valid("led-signal-basis"));
    TEST_ASSERT_TRUE(net_config_hostname_is_valid("a"));
    TEST_ASSERT_TRUE(net_config_hostname_is_valid("ESP32c3"));
    TEST_ASSERT_TRUE(net_config_hostname_is_valid("abcdefghijabcdefghijabcdefghij12"));  // 32
}

static void test_hostname_invalid(void)
{
    TEST_ASSERT_FALSE(net_config_hostname_is_valid(NULL));
    TEST_ASSERT_FALSE(net_config_hostname_is_valid(""));
    TEST_ASSERT_FALSE(net_config_hostname_is_valid("led_signal_basis"));
    TEST_ASSERT_FALSE(net_config_hostname_is_valid("-led"));
    TEST_ASSERT_FALSE(net_config_hostname_is_valid("led-"));
    TEST_ASSERT_FALSE(net_config_hostname_is_valid("led signal"));
    TEST_ASSERT_FALSE(net_config_hostname_is_valid("led.local"));
    TEST_ASSERT_FALSE(net_config_hostname_is_valid("abcdefghijabcdefghijabcdefghij123"));  // 33
}

static void test_validate_accepts_valid_config(void)
{
    net_config_t cfg = valid_config();
    TEST_ASSERT_EQUAL(NET_CONFIG_OK, net_config_validate(&cfg));
}

static void test_validate_allows_empty_ssid_and_password(void)
{
    net_config_t cfg = valid_config();
    cfg.ssid[0] = '\0';
    cfg.password[0] = '\0';
    TEST_ASSERT_EQUAL(NET_CONFIG_OK, net_config_validate(&cfg));
}

static void test_validate_rejects_short_password(void)
{
    net_config_t cfg = valid_config();
    strcpy(cfg.password, "1234567");
    TEST_ASSERT_EQUAL(NET_CONFIG_ERR_PASSWORD, net_config_validate(&cfg));
}

static void test_validate_rejects_bad_hostname(void)
{
    net_config_t cfg = valid_config();
    strcpy(cfg.hostname, "mit_unterstrich");
    TEST_ASSERT_EQUAL(NET_CONFIG_ERR_HOSTNAME, net_config_validate(&cfg));
}

static void test_validate_rejects_unterminated_fields(void)
{
    net_config_t cfg = valid_config();
    memset(cfg.ssid, 'x', sizeof(cfg.ssid));
    TEST_ASSERT_EQUAL(NET_CONFIG_ERR_SSID, net_config_validate(&cfg));
    cfg = valid_config();
    memset(cfg.password, 'x', sizeof(cfg.password));
    TEST_ASSERT_EQUAL(NET_CONFIG_ERR_PASSWORD, net_config_validate(&cfg));
    TEST_ASSERT_EQUAL(NET_CONFIG_ERR_SSID, net_config_validate(NULL));
}

static void test_fall_back_only_when_offline_long_enough(void)
{
    TEST_ASSERT_FALSE(net_should_fall_back(200000, 0, true, 120000));
    TEST_ASSERT_FALSE(net_should_fall_back(119999, 0, false, 120000));
    TEST_ASSERT_TRUE(net_should_fall_back(120000, 0, false, 120000));
}

static void test_fall_back_handles_wraparound(void)
{
    // Verbindung kurz vor dem Überlauf verloren, now danach
    TEST_ASSERT_FALSE(net_should_fall_back(1000, 0xFFFFFFFFu - 1000, false, 120000));
    TEST_ASSERT_TRUE(net_should_fall_back(119000, 0xFFFFFFFFu - 1000, false, 120000));
}

static void test_reason_text_known_codes(void)
{
    TEST_ASSERT_NOT_NULL(strstr(net_disconnect_reason_text(201), "nicht gefunden"));
    TEST_ASSERT_NOT_NULL(strstr(net_disconnect_reason_text(15), "Passwort"));
    TEST_ASSERT_NOT_NULL(strstr(net_disconnect_reason_text(204), "Passwort"));
    TEST_ASSERT_NOT_NULL(net_disconnect_reason_text(202));
}

static void test_reason_text_unknown_code(void)
{
    TEST_ASSERT_NULL(net_disconnect_reason_text(0));
    TEST_ASSERT_NULL(net_disconnect_reason_text(999));
}

static void test_ble_name_valid(void)
{
    TEST_ASSERT_TRUE(net_config_ble_name_is_valid("LED Signal"));
    TEST_ASSERT_TRUE(net_config_ble_name_is_valid("x"));
    TEST_ASSERT_TRUE(net_config_ble_name_is_valid("Kueche #2 (links)!"));
    TEST_ASSERT_TRUE(net_config_ble_name_is_valid("12345678901234567890"));
}

static void test_ble_name_invalid(void)
{
    TEST_ASSERT_FALSE(net_config_ble_name_is_valid(NULL));
    TEST_ASSERT_FALSE(net_config_ble_name_is_valid(""));
    TEST_ASSERT_FALSE(net_config_ble_name_is_valid("123456789012345678901"));
    TEST_ASSERT_FALSE(net_config_ble_name_is_valid(" LED"));
    TEST_ASSERT_FALSE(net_config_ble_name_is_valid("LED "));
    TEST_ASSERT_FALSE(net_config_ble_name_is_valid("LED\tSignal"));
    TEST_ASSERT_FALSE(net_config_ble_name_is_valid("K\xc3\xbc" "che"));  // Umlaut (UTF-8)
}

// Wird von test_main.c aufgerufen
void run_net_config_tests(void)
{
    RUN_TEST(test_hostname_valid);
    RUN_TEST(test_hostname_invalid);
    RUN_TEST(test_validate_accepts_valid_config);
    RUN_TEST(test_validate_allows_empty_ssid_and_password);
    RUN_TEST(test_validate_rejects_short_password);
    RUN_TEST(test_validate_rejects_bad_hostname);
    RUN_TEST(test_validate_rejects_unterminated_fields);
    RUN_TEST(test_fall_back_only_when_offline_long_enough);
    RUN_TEST(test_fall_back_handles_wraparound);
    RUN_TEST(test_reason_text_known_codes);
    RUN_TEST(test_reason_text_unknown_code);
    RUN_TEST(test_ble_name_valid);
    RUN_TEST(test_ble_name_invalid);
}
