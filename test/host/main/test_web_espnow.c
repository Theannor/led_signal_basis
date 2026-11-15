// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "web_server.h"
#include "web_form.h"
#include "Mockesp_http_server.h"
#include "Mocknvs.h"

// Tests für den ESP-NOW-Bereich der Einstellungen (POST /espnow) und den
// Empfänger-Modus (keine Farben per HTTP)

static int s_dummy_server;
#define FAKE_SERVER ((httpd_handle_t)&s_dummy_server)
#define FAKE_NVS    ((nvs_handle_t)6)

static const uint8_t OWN_MAC[6] = {0x02, 0x11, 0x22, 0x33, 0x44, 0x55};

static esp_err_t (*s_get_root)(httpd_req_t *req);
static esp_err_t (*s_get_status)(httpd_req_t *req);
static esp_err_t (*s_get_espnow)(httpd_req_t *req);
static esp_err_t (*s_get_colors)(httpd_req_t *req);
static esp_err_t (*s_put_colors)(httpd_req_t *req);
static esp_err_t (*s_post_espnow)(httpd_req_t *req);

static const char *s_body;
static char s_sent[8192];
static char s_status[40];
static int s_restarts;
static uint8_t s_saved[ESPNOW_CONFIG_BYTES_MAX];
static size_t s_saved_len;

static esp_err_t start_stub(httpd_handle_t *handle, const httpd_config_t *config, int cmock_num_calls)
{
    *handle = FAKE_SERVER;
    TEST_ASSERT_GREATER_OR_EQUAL(8, config->max_uri_handlers);
    return ESP_OK;
}

static esp_err_t register_stub(httpd_handle_t handle, const httpd_uri_t *uri, int cmock_num_calls)
{
    if (strcmp(uri->uri, "/") == 0 && uri->method == HTTP_GET) {
        s_get_root = uri->handler;
    } else if (strcmp(uri->uri, "/status") == 0 && uri->method == HTTP_GET) {
        s_get_status = uri->handler;
    } else if (strcmp(uri->uri, "/colors") == 0 && uri->method == HTTP_GET) {
        s_get_colors = uri->handler;
    } else if (strcmp(uri->uri, "/api/colors") == 0 && uri->method == HTTP_PUT) {
        s_put_colors = uri->handler;
    } else if (strcmp(uri->uri, "/espnow") == 0 && uri->method == HTTP_GET) {
        s_get_espnow = uri->handler;
    } else if (strcmp(uri->uri, "/espnow") == 0 && uri->method == HTTP_POST) {
        s_post_espnow = uri->handler;
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
    TEST_ASSERT_LESS_THAN(sizeof(s_sent), (size_t)len);
    memcpy(s_sent, buf, (size_t)len);
    s_sent[len] = '\0';
    return ESP_OK;
}

static esp_err_t set_status_stub(httpd_req_t *req, const char *status, int cmock_num_calls)
{
    strlcpy(s_status, status, sizeof(s_status));
    return ESP_OK;
}

static esp_err_t set_blob_stub(nvs_handle_t handle, const char *key, const void *value,
                               size_t length, int cmock_num_calls)
{
    TEST_ASSERT_EQUAL_STRING("espnow", key);
    TEST_ASSERT_LESS_OR_EQUAL(sizeof(s_saved), length);
    memcpy(s_saved, value, length);
    s_saved_len = length;
    return ESP_OK;
}

static void fake_restart(void)
{
    s_restarts++;
}

static void start_server(bool receiver_mode, const espnow_config_t *espnow)
{
    web_server_ctx_t ctx = {
        .client_mode = !receiver_mode,
        .port = 80,
        .config = {.ssid = "Heim", .hostname = "lampe"},
        .schedule_restart = fake_restart,
        .receiver_mode = receiver_mode,
        .espnow = *espnow,
        .espnow_window_s = 60,
    };
    memcpy(ctx.own_mac, OWN_MAC, 6);
    s_restarts = 0;
    s_saved_len = 0;
    s_sent[0] = s_status[0] = '\0';
    s_get_root = s_get_status = s_get_espnow = s_get_colors = s_put_colors = s_post_espnow = NULL;
    httpd_start_Stub(start_stub);
    httpd_register_uri_handler_Stub(register_stub);
    // Keine Cookie- und Accept-Language-Header -> deutsche Seiten
    httpd_req_get_hdr_value_str_IgnoreAndReturn(ESP_ERR_NOT_FOUND);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_start(&ctx));
    TEST_ASSERT_NOT_NULL(s_post_espnow);

    httpd_resp_set_status_Stub(set_status_stub);
    httpd_resp_set_type_IgnoreAndReturn(ESP_OK);
    httpd_resp_send_Stub(send_stub);
}

static void stop_server(void)
{
    httpd_stop_ExpectAndReturn(FAKE_SERVER, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_stop());
}

static void request(esp_err_t (*handler)(httpd_req_t *), const char *body)
{
    httpd_req_t req = {0};
    s_body = body;
    req.content_len = strlen(body);
    httpd_req_recv_Stub(recv_stub);
    TEST_ASSERT_EQUAL(ESP_OK, handler(&req));
}

static void expect_save(void)
{
    static nvs_handle_t handle = FAKE_NVS;
    nvs_open_ExpectAndReturn("led_signal", NVS_READWRITE, NULL, ESP_OK);
    nvs_open_IgnoreArg_out_handle();
    nvs_open_ReturnThruPtr_out_handle(&handle);
    nvs_set_blob_Stub(set_blob_stub);
    nvs_commit_ExpectAndReturn(FAKE_NVS, ESP_OK);
    nvs_close_Expect(FAKE_NVS);
}

static espnow_config_t gateway_default(void)
{
    espnow_config_t cfg;
    espnow_config_default(&cfg);
    return cfg;
}

static void test_render_espnow_card(void)
{
    char buf[6144];
    web_page_info_t info = {
        .client_mode = true, .hostname = "lampe",
        .own_mac = "02:11:22:33:44:55",
        .espnow_peers = "aa:bb:cc:dd:ee:02, 10:20:30:40:50:60",
        .espnow_gateway = "",
        .espnow_window_s = 60,
        .settings_page = WEB_SETTINGS_ESPNOW,
    };
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/espnow\" class=\"cur\">ESP-NOW</a>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "action=\"/espnow\""));
    TEST_ASSERT_NOT_NULL(strstr(buf, "value=\"gateway\" checked"));
    TEST_ASSERT_NULL(strstr(buf, "value=\"receiver\" checked"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "value=\"aa:bb:cc:dd:ee:02, 10:20:30:40:50:60\""));
    TEST_ASSERT_NULL(strstr(buf, "<dt>ESP-NOW-MAC</dt>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "href=\"/colors\""));
    TEST_ASSERT_GREATER_THAN(0, web_status_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<dt>ESP-NOW-MAC</dt><dd>02:11:22:33:44:55</dd>"));

    // Empfänger: Rolle markiert, Menü ohne Farben
    info.espnow_receiver = true;
    info.receiver_mode = true;
    info.client_mode = false;
    info.espnow_gateway = "aa:bb:cc:dd:ee:02";
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "value=\"receiver\" checked"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "name=\"gateway\" maxlength=\"17\" value=\"aa:bb:cc:dd:ee:02\""));
    TEST_ASSERT_NULL(strstr(buf, "href=\"/colors\""));
    TEST_ASSERT_GREATER_THAN(0, web_status_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NOT_NULL(strstr(buf, "ESP-NOW-Empfänger"));
    TEST_ASSERT_NULL(strstr(buf, "href=\"/colors\""));

    // Ohne eigene MAC kein ESP-NOW-Bereich
    info.own_mac = NULL;
    TEST_ASSERT_GREATER_THAN(0, web_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NULL(strstr(buf, "/espnow"));
    TEST_ASSERT_GREATER_THAN(0, web_status_page_render(buf, sizeof(buf), &info));
    TEST_ASSERT_NULL(strstr(buf, "ESP-NOW-MAC"));
}

static void test_page_with_full_peer_list_fits(void)
{
    espnow_config_t cfg = gateway_default();
    for (size_t i = 0; i < ESPNOW_PEERS_MAX; i++) {
        const uint8_t mac[6] = {0x02, 0, 0, 0, 0, (uint8_t)(i + 1)};
        memcpy(cfg.peers[i], mac, 6);
    }
    cfg.peer_count = ESPNOW_PEERS_MAX;
    start_server(false, &cfg);
    httpd_req_t req = {0};
    TEST_ASSERT_NOT_NULL(s_get_espnow);
    TEST_ASSERT_EQUAL(ESP_OK, s_get_espnow(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "02:00:00:00:00:0a\""));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "</main></body></html>"));
    stop_server();
}

static void test_post_gateway_with_peers(void)
{
    espnow_config_t cfg = gateway_default();
    start_server(false, &cfg);
    expect_save();
    request(s_post_espnow,
            "espnow_role=gateway&peers=AA%3ABB%3ACC%3ADD%3AEE%3A02%2C+10-20-30-40-50-60&gateway=");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    espnow_config_t saved;
    TEST_ASSERT_TRUE(espnow_config_from_bytes(s_saved, s_saved_len, &saved));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_GATEWAY, saved.role);
    TEST_ASSERT_EQUAL_size_t(2, saved.peer_count);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"aa:bb:cc:dd:ee:02, 10:20:30:40:50:60\""));
    stop_server();
}

static void test_post_receiver_with_gateway(void)
{
    espnow_config_t cfg = gateway_default();
    start_server(false, &cfg);
    expect_save();
    request(s_post_espnow, "espnow_role=receiver&peers=&gateway=+aa%3Abb%3Acc%3Add%3Aee%3A02+");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_EQUAL_INT(1, s_restarts);
    espnow_config_t saved;
    TEST_ASSERT_TRUE(espnow_config_from_bytes(s_saved, s_saved_len, &saved));
    TEST_ASSERT_EQUAL(ESPNOW_ROLE_RECEIVER, saved.role);
    const uint8_t gw[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};
    TEST_ASSERT_EQUAL_UINT8_ARRAY(gw, saved.gateway, 6);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"receiver\" checked"));
    stop_server();
}

static void test_post_rejects_invalid_input(void)
{
    espnow_config_t cfg = gateway_default();
    start_server(false, &cfg);
    request(s_post_espnow, "espnow_role=receiver&peers=&gateway=");          // Gateway fehlt
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "Gateway:"));
    request(s_post_espnow, "espnow_role=receiver&gateway=02%3A11%3A22%3A33%3A44%3A55");  // eigene MAC
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    request(s_post_espnow, "espnow_role=gateway&peers=02%3A11%3A22%3A33%3A44%3A55");     // eigene MAC
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "Empfänger:"));
    request(s_post_espnow, "espnow_role=gateway&peers=kaputt");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    request(s_post_espnow, "espnow_role=gateway&gateway=ff%3Aff%3Aff%3Aff%3Aff%3Aff");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    request(s_post_espnow, "espnow_role=chef&peers=");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    request(s_post_espnow, "peers=");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

static void test_post_reports_nvs_error(void)
{
    espnow_config_t cfg = gateway_default();
    start_server(false, &cfg);
    nvs_open_ExpectAnyArgsAndReturn(ESP_ERR_NVS_NOT_INITIALIZED);
    request(s_post_espnow, "espnow_role=gateway&peers=");
    TEST_ASSERT_EQUAL_STRING("500 Internal Server Error", s_status);
    TEST_ASSERT_EQUAL_INT(0, s_restarts);
    stop_server();
}

static void test_receiver_mode_blocks_colors(void)
{
    espnow_config_t cfg = gateway_default();
    cfg.role = ESPNOW_ROLE_RECEIVER;
    const uint8_t gw[6] = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0x02};
    memcpy(cfg.gateway, gw, 6);
    start_server(true, &cfg);

    httpd_req_t req = {0};
    TEST_ASSERT_EQUAL(ESP_OK, s_get_colors(&req));
    TEST_ASSERT_EQUAL_STRING("403 Forbidden", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "ESP-NOW-Empfänger"));

    // Kein NVS-Zugriff: die Palette wird gar nicht erst gelesen
    request(s_put_colors, "{\"colors\":[\"#ff0000\"]}");
    TEST_ASSERT_EQUAL_STRING("403 Forbidden", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "\"error\""));

    TEST_ASSERT_EQUAL(ESP_OK, s_get_espnow(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NULL(strstr(s_sent, "href=\"/colors\""));
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "value=\"aa:bb:cc:dd:ee:02\""));

    // Startseite des Empfängers: WLAN-Einstellungen, keine Farben
    TEST_ASSERT_EQUAL(ESP_OK, s_get_root(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "action=\"/config\""));

    // Status-Seite bleibt auch für Empfänger erreichbar
    TEST_ASSERT_NOT_NULL(s_get_status);
    TEST_ASSERT_EQUAL(ESP_OK, s_get_status(&req));
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "Access Point, danach ESP-NOW-Empfänger"));
    TEST_ASSERT_NULL(strstr(s_sent, "href=\"/colors\""));
    stop_server();
}

// Wird von test_main.c aufgerufen
void run_web_espnow_tests(void)
{
    RUN_TEST(test_render_espnow_card);
    RUN_TEST(test_page_with_full_peer_list_fits);
    RUN_TEST(test_post_gateway_with_peers);
    RUN_TEST(test_post_receiver_with_gateway);
    RUN_TEST(test_post_rejects_invalid_input);
    RUN_TEST(test_post_reports_nvs_error);
    RUN_TEST(test_receiver_mode_blocks_colors);
}
