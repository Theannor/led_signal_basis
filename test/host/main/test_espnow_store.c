#include <string.h>
#include "unity.h"
#include "espnow_store.h"
#include "Mocknvs.h"

#define FAKE_HANDLE ((nvs_handle_t)13)

static const uint8_t MAC_A[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x01};

// Inhalt, den der nvs_get_blob-Stub liefert, und was nvs_set_blob bekam
static uint8_t s_blob[ESPNOW_CONFIG_BYTES_MAX + 8];
static size_t s_blob_len;
static esp_err_t s_get_result;

static void expect_open_ok(void)
{
    static nvs_handle_t handle = FAKE_HANDLE;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
}

static esp_err_t get_blob_stub(nvs_handle_t handle, const char *key, void *out, size_t *length,
                               int cmock_num_calls)
{
    TEST_ASSERT_EQUAL(FAKE_HANDLE, handle);
    TEST_ASSERT_EQUAL_STRING("espnow", key);
    if (s_get_result != ESP_OK) {
        return s_get_result;
    }
    if (s_blob_len > *length) {
        return ESP_ERR_NVS_INVALID_LENGTH;
    }
    memcpy(out, s_blob, s_blob_len);
    *length = s_blob_len;
    return ESP_OK;
}

static esp_err_t set_blob_stub(nvs_handle_t handle, const char *key, const void *value,
                               size_t length, int cmock_num_calls)
{
    TEST_ASSERT_EQUAL(FAKE_HANDLE, handle);
    TEST_ASSERT_EQUAL_STRING("espnow", key);
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(s_blob), length);
    memcpy(s_blob, value, length);
    s_blob_len = length;
    return ESP_OK;
}

static esp_err_t load(esp_err_t get_result, espnow_config_t *out)
{
    s_get_result = get_result;
    expect_open_ok();
    nvs_get_blob_Stub(get_blob_stub);
    nvs_close_Expect(FAKE_HANDLE);
    return espnow_store_load(out);
}

static espnow_config_t receiver_cfg(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    cfg.role = ESPNOW_ROLE_RECEIVER;
    memcpy(cfg.gateway, MAC_A, 6);
    return cfg;
}

static void test_save_then_load(void)
{
    espnow_config_t cfg = receiver_cfg();
    expect_open_ok();
    nvs_set_blob_Stub(set_blob_stub);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, espnow_store_save(&cfg));

    espnow_config_t out;
    TEST_ASSERT_EQUAL(ESP_OK, load(ESP_OK, &out));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_RECEIVER, out.role);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(MAC_A, out.gateway, 6);
    TEST_ASSERT_EQUAL_size_t(0, out.peer_count);
}

static void test_load_missing_gives_default(void)
{
    espnow_config_t out = receiver_cfg();
    TEST_ASSERT_EQUAL(ESP_OK, load(ESP_ERR_NVS_NOT_FOUND, &out));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_GATEWAY, out.role);
    TEST_ASSERT_FALSE(espnow_mac_is_set(out.gateway));
}

static void test_load_invalid_gives_default(void)
{
    // Rolle Empfänger ohne Gateway ist ungültig
    const uint8_t blob[] = {1, ESPNOW_ROLE_RECEIVER, 0, 0, 0, 0, 0, 0, 0};
    memcpy(s_blob, blob, sizeof(blob));
    s_blob_len = sizeof(blob);
    espnow_config_t out;
    TEST_ASSERT_EQUAL(ESP_OK, load(ESP_OK, &out));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_GATEWAY, out.role);

    s_blob[0] = 77;  // unbekanntes Format
    TEST_ASSERT_EQUAL(ESP_OK, load(ESP_OK, &out));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_GATEWAY, out.role);

    TEST_ASSERT_EQUAL(ESP_OK, load(ESP_ERR_NVS_INVALID_LENGTH, &out));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_GATEWAY, out.role);
}

static void test_load_open_error_and_bad_args(void)
{
    espnow_config_t out = receiver_cfg();
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_INITIALIZED, espnow_store_load(&out));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_GATEWAY, out.role);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, espnow_store_load(NULL));
}

static void test_save_rejects_invalid_and_reports_errors(void)
{
    espnow_config_t cfg = receiver_cfg();
    memset(cfg.gateway, 0, 6);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, espnow_store_save(&cfg));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, espnow_store_save(NULL));

    cfg = receiver_cfg();
    expect_open_ok();
    nvs_set_blob_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_ENOUGH_SPACE, espnow_store_save(&cfg));
}

// Wird von test_main.c aufgerufen
void run_espnow_store_tests(void)
{
    RUN_TEST(test_save_then_load);
    RUN_TEST(test_load_missing_gives_default);
    RUN_TEST(test_load_invalid_gives_default);
    RUN_TEST(test_load_open_error_and_bad_args);
    RUN_TEST(test_save_rejects_invalid_and_reports_errors);
}
