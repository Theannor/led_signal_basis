// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "config_store.h"
#include "Mocknvs.h"

#define FAKE_HANDLE  ((nvs_handle_t)7)
#define DEFAULT_HOST "led-signal-basis"

static void expect_open_ok(void)
{
    static nvs_handle_t handle = FAKE_HANDLE;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
}

// nvs_get_str für key: value != NULL -> gefunden, sonst NOT_FOUND
static void expect_get(const char *key, const char *value)
{
    // Jeder Aufruf braucht eigenen Speicher, weil CMock nur Zeiger merkt
    static char buffers[3][80];
    static size_t lengths[3];
    static int next = 0;
    int i = next++ % 3;

    if (value == NULL) {
        nvs_get_str_ExpectAndReturn(FAKE_HANDLE, key, NULL, NULL, ESP_ERR_NVS_NOT_FOUND);
        nvs_get_str_IgnoreArg_out_value();
        nvs_get_str_IgnoreArg_length();
        return;
    }
    lengths[i] = strlen(value) + 1;
    memcpy(buffers[i], value, lengths[i]);
    nvs_get_str_ExpectAndReturn(FAKE_HANDLE, key, NULL, NULL, ESP_OK);
    nvs_get_str_IgnoreArg_out_value();
    nvs_get_str_IgnoreArg_length();
    nvs_get_str_ReturnArrayThruPtr_out_value(buffers[i], lengths[i]);
    nvs_get_str_ReturnThruPtr_length(&lengths[i]);
}

static void test_load_stored_values(void)
{
    net_config_t cfg;
    expect_open_ok();
    expect_get("sta_ssid", "Heim");
    expect_get("sta_pass", "geheim123");
    expect_get("hostname", "lampe");
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, config_store_load(&cfg, DEFAULT_HOST));
    TEST_ASSERT_EQUAL_STRING("Heim", cfg.ssid);
    TEST_ASSERT_EQUAL_STRING("geheim123", cfg.password);
    TEST_ASSERT_EQUAL_STRING("lampe", cfg.hostname);
}

static void test_load_missing_values_gives_defaults(void)
{
    net_config_t cfg;
    memset(&cfg, 'x', sizeof(cfg));
    expect_open_ok();
    expect_get("sta_ssid", NULL);
    expect_get("sta_pass", NULL);
    expect_get("hostname", NULL);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, config_store_load(&cfg, DEFAULT_HOST));
    TEST_ASSERT_EQUAL_STRING("", cfg.ssid);
    TEST_ASSERT_EQUAL_STRING("", cfg.password);
    TEST_ASSERT_EQUAL_STRING(DEFAULT_HOST, cfg.hostname);
}

static void test_load_invalid_values_gives_defaults(void)
{
    net_config_t cfg;
    expect_open_ok();
    expect_get("sta_ssid", "Heim");
    expect_get("sta_pass", "kurz");
    expect_get("hostname", "lampe");
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, config_store_load(&cfg, DEFAULT_HOST));
    TEST_ASSERT_EQUAL_STRING("", cfg.ssid);
    TEST_ASSERT_EQUAL_STRING(DEFAULT_HOST, cfg.hostname);
}

static void test_load_open_error_still_fills_defaults(void)
{
    net_config_t cfg;
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_INITIALIZED, config_store_load(&cfg, DEFAULT_HOST));
    TEST_ASSERT_EQUAL_STRING("", cfg.ssid);
    TEST_ASSERT_EQUAL_STRING(DEFAULT_HOST, cfg.hostname);
}

static void test_save_writes_all_fields(void)
{
    net_config_t cfg = {.ssid = "Heim", .password = "geheim123", .hostname = "lampe"};
    expect_open_ok();
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "sta_ssid", "Heim", ESP_OK);
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "sta_pass", "geheim123", ESP_OK);
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "hostname", "lampe", ESP_OK);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, config_store_save(&cfg));
}

static void test_save_rejects_invalid_config(void)
{
    net_config_t cfg = {.ssid = "Heim", .password = "kurz", .hostname = "lampe"};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, config_store_save(&cfg));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, config_store_save(NULL));
}

static void test_save_stops_on_write_error(void)
{
    net_config_t cfg = {.ssid = "Heim", .password = "", .hostname = "lampe"};
    expect_open_ok();
    nvs_set_str_ExpectAndReturn(FAKE_HANDLE, "sta_ssid", "Heim", ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_ENOUGH_SPACE, config_store_save(&cfg));
}

// Wird von test_main.c aufgerufen
void run_config_store_tests(void)
{
    RUN_TEST(test_load_stored_values);
    RUN_TEST(test_load_missing_values_gives_defaults);
    RUN_TEST(test_load_invalid_values_gives_defaults);
    RUN_TEST(test_load_open_error_still_fills_defaults);
    RUN_TEST(test_save_writes_all_fields);
    RUN_TEST(test_save_rejects_invalid_config);
    RUN_TEST(test_save_stops_on_write_error);
}
