#include <string.h>
#include "unity.h"
#include "palette_store.h"
#include "Mocknvs.h"

#define FAKE_HANDLE ((nvs_handle_t)9)

// Inhalt, den der nvs_get_blob-Stub liefert, und was nvs_set_blob bekam
static uint8_t s_blob[64];
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
    TEST_ASSERT_EQUAL_STRING("palette", key);
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
    TEST_ASSERT_EQUAL_STRING("palette", key);
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(s_blob), length);
    memcpy(s_blob, value, length);
    s_blob_len = length;
    return ESP_OK;
}

// palette_store_load mit vorgegebenem Blob bzw. Fehler
static esp_err_t load(const uint8_t *blob, size_t len, esp_err_t get_result, led_palette_t *out)
{
    if (len > 0) {
        memcpy(s_blob, blob, len);
    }
    s_blob_len = len;
    s_get_result = get_result;
    expect_open_ok();
    nvs_get_blob_Stub(get_blob_stub);
    nvs_close_Expect(FAKE_HANDLE);
    return palette_store_load(out);
}

static void test_load_stored_palette(void)
{
    const uint8_t blob[] = {2, 255, 0, 0, 1, 2, 3};
    led_palette_t p;
    TEST_ASSERT_EQUAL(ESP_OK, load(blob, sizeof(blob), ESP_OK, &p));
    TEST_ASSERT_EQUAL_size_t(2, p.count);
    TEST_ASSERT_EQUAL_UINT8(255, p.colors[0].r);
    TEST_ASSERT_EQUAL_UINT8(1, p.colors[1].r);
    TEST_ASSERT_EQUAL_UINT8(2, p.colors[1].g);
    TEST_ASSERT_EQUAL_UINT8(3, p.colors[1].b);
}

static void test_load_missing_gives_empty(void)
{
    led_palette_t p = {.count = 5};
    TEST_ASSERT_EQUAL(ESP_OK, load(NULL, 0, ESP_ERR_NVS_NOT_FOUND, &p));
    TEST_ASSERT_EQUAL_size_t(0, p.count);
}

static void test_load_invalid_gives_empty(void)
{
    led_palette_t p;
    // Länge passt nicht zur Anzahl
    const uint8_t wrong_len[] = {2, 255, 0, 0};
    TEST_ASSERT_EQUAL(ESP_OK, load(wrong_len, sizeof(wrong_len), ESP_OK, &p));
    TEST_ASSERT_EQUAL_size_t(0, p.count);
    // Zu viele Farben
    const uint8_t too_many[] = {LED_PALETTE_MAX + 1};
    TEST_ASSERT_EQUAL(ESP_OK, load(too_many, sizeof(too_many), ESP_OK, &p));
    TEST_ASSERT_EQUAL_size_t(0, p.count);
    // Blob größer als der Puffer
    TEST_ASSERT_EQUAL(ESP_OK, load(NULL, 0, ESP_ERR_NVS_INVALID_LENGTH, &p));
    TEST_ASSERT_EQUAL_size_t(0, p.count);
}

static void test_load_open_error(void)
{
    led_palette_t p = {.count = 5};
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_INITIALIZED, palette_store_load(&p));
    TEST_ASSERT_EQUAL_size_t(0, p.count);
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, palette_store_load(NULL));
}

static void test_save_writes_blob(void)
{
    led_palette_t p = {.colors = {{.r = 9, .g = 8, .b = 7}}, .count = 1};
    expect_open_ok();
    nvs_set_blob_Stub(set_blob_stub);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, palette_store_save(&p));
    const uint8_t expected[] = {1, 9, 8, 7};
    TEST_ASSERT_EQUAL_size_t(sizeof(expected), s_blob_len);
    TEST_ASSERT_EQUAL_MEMORY(expected, s_blob, sizeof(expected));
}

static void test_save_empty_and_round_trip(void)
{
    led_palette_t p = {.count = 0}, back;
    expect_open_ok();
    nvs_set_blob_Stub(set_blob_stub);
    nvs_commit_ExpectAndReturn(FAKE_HANDLE, ESP_OK);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_OK, palette_store_save(&p));
    TEST_ASSERT_EQUAL_size_t(1, s_blob_len);
    uint8_t saved[64];
    memcpy(saved, s_blob, s_blob_len);
    TEST_ASSERT_EQUAL(ESP_OK, load(saved, s_blob_len, ESP_OK, &back));
    TEST_ASSERT_EQUAL_size_t(0, back.count);
}

static void test_save_rejects_invalid_and_reports_errors(void)
{
    led_palette_t p = {.count = LED_PALETTE_MAX + 1};
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, palette_store_save(&p));
    TEST_ASSERT_EQUAL(ESP_ERR_INVALID_ARG, palette_store_save(NULL));

    p.count = 1;
    expect_open_ok();
    nvs_set_blob_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    nvs_close_Expect(FAKE_HANDLE);
    TEST_ASSERT_EQUAL(ESP_ERR_NVS_NOT_ENOUGH_SPACE, palette_store_save(&p));
}

// Wird von test_main.c aufgerufen
void run_palette_store_tests(void)
{
    RUN_TEST(test_load_stored_palette);
    RUN_TEST(test_load_missing_gives_empty);
    RUN_TEST(test_load_invalid_gives_empty);
    RUN_TEST(test_load_open_error);
    RUN_TEST(test_save_writes_blob);
    RUN_TEST(test_save_empty_and_round_trip);
    RUN_TEST(test_save_rejects_invalid_and_reports_errors);
}
