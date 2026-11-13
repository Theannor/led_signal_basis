#include <stdio.h>
#include <string.h>
#include "unity.h"
#include "web_server.h"
#include "web_form.h"
#include "Mockesp_http_server.h"
#include "Mocknvs.h"

static int s_dummy_server;
#define FAKE_SERVER ((httpd_handle_t)&s_dummy_server)
#define FAKE_NVS    ((nvs_handle_t)4)

// Vom Stub gemerkte Handler
static esp_err_t (*s_colors_page)(httpd_req_t *req);
static esp_err_t (*s_style)(httpd_req_t *req);
static esp_err_t (*s_api_get)(httpd_req_t *req);
static esp_err_t (*s_api_put)(httpd_req_t *req);

static const char *s_body;
static char s_sent[4096];
static char s_status[40];
static char s_type[40];
static led_palette_t s_changed;
static int s_changes;
static uint8_t s_saved_blob[64];
static size_t s_saved_len;

static esp_err_t start_stub(httpd_handle_t *handle, const httpd_config_t *config, int cmock_num_calls)
{
    *handle = FAKE_SERVER;
    return ESP_OK;
}

static esp_err_t register_stub(httpd_handle_t handle, const httpd_uri_t *uri, int cmock_num_calls)
{
    if (strcmp(uri->uri, "/colors") == 0 && uri->method == HTTP_GET) {
        s_colors_page = uri->handler;
    } else if (strcmp(uri->uri, "/style.css") == 0 && uri->method == HTTP_GET) {
        s_style = uri->handler;
    } else if (strcmp(uri->uri, "/api/colors") == 0 && uri->method == HTTP_GET) {
        s_api_get = uri->handler;
    } else if (strcmp(uri->uri, "/api/colors") == 0 && uri->method == HTTP_PUT) {
        s_api_put = uri->handler;
    }
    return ESP_OK;
}

static int recv_stub(httpd_req_t *req, char *buf, size_t len, int cmock_num_calls)
{
    static size_t offset;
    if (cmock_num_calls == 0) {
        offset = 0;
    }
    size_t rest = strlen(s_body) - offset;
    size_t n = rest < len ? rest : len;
    memcpy(buf, s_body + offset, n);
    offset += n;
    return (int)n;
}

static esp_err_t send_stub(httpd_req_t *req, const char *buf, ssize_t len, int cmock_num_calls)
{
    memcpy(s_sent, buf, (size_t)len);
    s_sent[len] = '\0';
    return ESP_OK;
}

static esp_err_t set_status_stub(httpd_req_t *req, const char *status, int cmock_num_calls)
{
    strlcpy(s_status, status, sizeof(s_status));
    return ESP_OK;
}

static esp_err_t set_type_stub(httpd_req_t *req, const char *type, int cmock_num_calls)
{
    strlcpy(s_type, type, sizeof(s_type));
    return ESP_OK;
}

static esp_err_t set_blob_stub(nvs_handle_t handle, const char *key, const void *value,
                               size_t length, int cmock_num_calls)
{
    TEST_ASSERT_EQUAL_STRING("palette", key);
    memcpy(s_saved_blob, value, length);
    s_saved_len = length;
    return ESP_OK;
}

static char s_hdr[80];
static esp_err_t set_hdr_stub(httpd_req_t *req, const char *field, const char *value,
                              int cmock_num_calls)
{
    snprintf(s_hdr, sizeof(s_hdr), "%s: %s", field, value);
    return ESP_OK;
}

static void on_changed(const led_palette_t *palette)
{
    s_changed = *palette;
    s_changes++;
}

// Server mit einer roten Farbe in der Palette starten
static void start_server(void)
{
    web_server_ctx_t ctx = {
        .client_mode = true,
        .port = 80,
        .config = {.ssid = "Heim", .hostname = "lampe"},
        .palette = {.colors = {{.r = 255}}, .count = 1},
        .on_palette_changed = on_changed,
    };
    s_changes = 0;
    s_saved_len = 0;
    s_sent[0] = s_status[0] = s_type[0] = '\0';
    s_colors_page = s_style = s_api_get = s_api_put = NULL;
    httpd_start_Stub(start_stub);
    httpd_register_uri_handler_Stub(register_stub);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_start(&ctx));
    TEST_ASSERT_NOT_NULL(s_colors_page);
    TEST_ASSERT_NOT_NULL(s_api_get);
    TEST_ASSERT_NOT_NULL(s_api_put);

    httpd_resp_set_status_Stub(set_status_stub);
    httpd_resp_set_type_Stub(set_type_stub);
    httpd_resp_send_Stub(send_stub);
}

static void stop_server(void)
{
    httpd_stop_ExpectAndReturn(FAKE_SERVER, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_stop());
}

static void put(const char *body)
{
    httpd_req_t req = {0};
    s_body = body;
    req.content_len = strlen(body);
    httpd_req_recv_Stub(recv_stub);
    TEST_ASSERT_EQUAL(ESP_OK, s_api_put(&req));
}

static void expect_save(esp_err_t set_result)
{
    static nvs_handle_t handle = FAKE_NVS;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
    if (set_result == ESP_OK) {
        nvs_set_blob_Stub(set_blob_stub);
        nvs_commit_ExpectAndReturn(FAKE_NVS, ESP_OK);
    } else {
        nvs_set_blob_ExpectAnyArgsAndReturn(set_result);
    }
    nvs_close_Expect(FAKE_NVS);
}

static void test_colors_page_is_served(void)
{
    start_server();
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_colors_page(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_type, "text/html"));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "/api/colors"));
    stop_server();
}

static void test_style_is_served_with_cache(void)
{
    start_server();
    TEST_ASSERT_NOT_NULL(s_style);
    httpd_resp_set_hdr_Stub(set_hdr_stub);
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_style(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_STRING("text/css", s_type);
    TEST_ASSERT_EQUAL_STRING("Cache-Control: max-age=3600", s_hdr);
    TEST_ASSERT_EQUAL_STRING(web_style_css(), s_sent);
    stop_server();
}

static void test_style_css_content(void)
{
    const char *css = web_style_css();
    TEST_ASSERT_NOT_NULL(css);
    TEST_ASSERT_NOT_NULL(strstr(css, ":root{"));
    TEST_ASSERT_NOT_NULL(strstr(css, "prefers-color-scheme:dark"));
    TEST_ASSERT_LESS_THAN(sizeof(s_sent), strlen(css));
}

static void test_pages_link_stylesheet(void)
{
    char buf[4096];
    // Reserve für spätere Änderungen: Farben-Seite höchstens 3/4 des Puffers
    int n = web_colors_page_render(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_LESS_THAN(3 * 4096 / 4, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "href=\"/style.css\""));
    web_page_info_t info = {.ip = "1.2.3.4", .hostname = "h", .ssid = "s"};
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "href=\"/style.css\""));
}

static void test_api_get_returns_palette(void)
{
    start_server();
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_api_get(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_STRING("application/json", s_type);
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[\"#ff0000\"]}", s_sent);
    stop_server();
}

static void test_api_put_saves_and_notifies(void)
{
    start_server();
    expect_save(ESP_OK);
    put("{\"colors\":[\"#00ff00\",\"#0000FF\"]}");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[\"#00ff00\",\"#0000ff\"]}", s_sent);
    const uint8_t blob[] = {2, 0, 255, 0, 0, 0, 255};
    TEST_ASSERT_EQUAL_size_t(sizeof(blob), s_saved_len);
    TEST_ASSERT_EQUAL_MEMORY(blob, s_saved_blob, sizeof(blob));
    TEST_ASSERT_EQUAL_INT(1, s_changes);
    TEST_ASSERT_EQUAL_size_t(2, s_changed.count);
    TEST_ASSERT_EQUAL_UINT8(255, s_changed.colors[1].b);

    // GET liefert danach die neue Palette
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_api_get(&req));
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[\"#00ff00\",\"#0000ff\"]}", s_sent);
    stop_server();
}

static void test_api_put_empty_list(void)
{
    start_server();
    expect_save(ESP_OK);
    put("{\"colors\":[]}");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[]}", s_sent);
    TEST_ASSERT_EQUAL_size_t(0, s_changed.count);
    stop_server();
}

static void test_api_put_rejects_invalid_json(void)
{
    start_server();
    put("{\"colors\":[\"rot\"]}");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "\"error\""));
    TEST_ASSERT_EQUAL_INT(0, s_changes);
    stop_server();
}

static void test_api_put_rejects_large_body(void)
{
    start_server();
    httpd_req_t req = {0};
    req.content_len = 5000;
    TEST_ASSERT_EQUAL(ESP_OK, s_api_put(&req));
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_changes);
    stop_server();
}

static void test_api_put_reports_save_error(void)
{
    start_server();
    expect_save(ESP_ERR_NVS_NOT_ENOUGH_SPACE);
    put("[\"#ffffff\"]");
    TEST_ASSERT_EQUAL_STRING("500 Internal Server Error", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_changes);
    // Palette bleibt unverändert
    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_api_get(&req));
    TEST_ASSERT_EQUAL_STRING("{\"colors\":[\"#ff0000\"]}", s_sent);
    stop_server();
}

static void test_colors_page_render(void)
{
    char buf[4096];
    int n = web_colors_page_render(buf, sizeof(buf));
    TEST_ASSERT_GREATER_THAN(0, n);
    TEST_ASSERT_EQUAL_size_t((size_t)n, strlen(buf));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<h1>Farben</h1>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "fetch('/api/colors'"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "method:'PUT'"));
    // Menü mit beiden Seiten, Farben markiert
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/\">Einstellungen</a>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/colors\" class=\"cur\">Farben</a>"));
    char small[200];
    TEST_ASSERT_EQUAL_INT(-1, web_colors_page_render(small, sizeof(small)));
    TEST_ASSERT_EQUAL_INT(-1, web_colors_page_render(NULL, 10));
}

static void test_wifi_page_has_menu(void)
{
    char buf[4096];
    web_page_info_t info = {.ip = "1.2.3.4", .hostname = "h", .ssid = "s"};
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/\" class=\"cur\">Einstellungen</a>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/colors\">Farben</a>"));
}

// Wird von test_main.c aufgerufen
void run_web_api_tests(void)
{
    RUN_TEST(test_colors_page_is_served);
    RUN_TEST(test_api_get_returns_palette);
    RUN_TEST(test_api_put_saves_and_notifies);
    RUN_TEST(test_api_put_empty_list);
    RUN_TEST(test_api_put_rejects_invalid_json);
    RUN_TEST(test_api_put_rejects_large_body);
    RUN_TEST(test_api_put_reports_save_error);
    RUN_TEST(test_colors_page_render);
    RUN_TEST(test_wifi_page_has_menu);
    RUN_TEST(test_style_is_served_with_cache);
    RUN_TEST(test_style_css_content);
    RUN_TEST(test_pages_link_stylesheet);
}
