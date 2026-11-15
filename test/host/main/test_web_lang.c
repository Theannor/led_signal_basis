// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "unity.h"
#include "web_lang.h"
#include "web_form.h"
#include "web_server.h"
#include "Mockesp_http_server.h"

static int s_dummy_server;
#define FAKE_SERVER ((httpd_handle_t)&s_dummy_server)

// Vom Stub gemerkte Handler
static esp_err_t (*s_get_root)(httpd_req_t *req);
static esp_err_t (*s_get_colors)(httpd_req_t *req);
static esp_err_t (*s_post_config)(httpd_req_t *req);
static esp_err_t (*s_put_colors)(httpd_req_t *req);

static char s_sent[8192];
static char s_status[40];
static const char *s_body;
// Header der Anfrage (NULL = fehlt)
static const char *s_cookie;
static const char *s_accept;

static esp_err_t start_stub(httpd_handle_t *handle, const httpd_config_t *config, int cmock_num_calls)
{
    *handle = FAKE_SERVER;
    return ESP_OK;
}

static esp_err_t register_stub(httpd_handle_t handle, const httpd_uri_t *uri, int cmock_num_calls)
{
    if (strcmp(uri->uri, "/") == 0) {
        s_get_root = uri->handler;
    } else if (strcmp(uri->uri, "/colors") == 0) {
        s_get_colors = uri->handler;
    } else if (strcmp(uri->uri, "/config") == 0) {
        s_post_config = uri->handler;
    } else if (strcmp(uri->uri, "/api/colors") == 0 && uri->method == HTTP_PUT) {
        s_put_colors = uri->handler;
    }
    return ESP_OK;
}

static esp_err_t hdr_stub(httpd_req_t *req, const char *field, char *val, size_t val_size,
                          int cmock_num_calls)
{
    const char *value = NULL;
    if (strcmp(field, "Cookie") == 0) {
        value = s_cookie;
    } else if (strcmp(field, "Accept-Language") == 0) {
        value = s_accept;
    }
    if (value == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    strlcpy(val, value, val_size);
    return strlen(value) < val_size ? ESP_OK : ESP_ERR_HTTPD_RESULT_TRUNC;
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

static int recv_stub(httpd_req_t *req, char *buf, size_t len, int cmock_num_calls)
{
    size_t n = strlen(s_body);
    memcpy(buf, s_body, n);
    return (int)n;
}

// Server starten; cookie und accept sind die Header aller folgenden Anfragen
static void start_server(const char *cookie, const char *accept)
{
    web_server_ctx_t ctx = {
        .client_mode = true,
        .port = 80,
        .config = {.ssid = "Heim", .hostname = "lampe"},
    };
    s_cookie = cookie;
    s_accept = accept;
    s_sent[0] = s_status[0] = '\0';
    httpd_start_Stub(start_stub);
    httpd_register_uri_handler_Stub(register_stub);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_start(&ctx));
    httpd_req_get_hdr_value_str_Stub(hdr_stub);
    httpd_resp_set_status_Stub(set_status_stub);
    httpd_resp_set_type_IgnoreAndReturn(ESP_OK);
    httpd_resp_send_Stub(send_stub);
    httpd_req_recv_Stub(recv_stub);
}

static void stop_server(void)
{
    httpd_stop_ExpectAndReturn(FAKE_SERVER, ESP_OK);
    TEST_ASSERT_EQUAL(ESP_OK, web_server_stop());
}

// Anfrage mit Body an handler schicken
static void request(esp_err_t (*handler)(httpd_req_t *req), const char *body)
{
    TEST_ASSERT_NOT_NULL(handler);
    httpd_req_t req = {0};
    s_body = body;
    req.content_len = strlen(body);
    TEST_ASSERT_EQUAL(ESP_OK, handler(&req));
}

static void test_lang_parse_and_code(void)
{
    web_lang_t lang = WEB_LANG_EN;
    TEST_ASSERT_TRUE(web_lang_parse("de", &lang));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, lang);
    TEST_ASSERT_TRUE(web_lang_parse("EN", &lang));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, lang);
    TEST_ASSERT_FALSE(web_lang_parse("fr", &lang));
    TEST_ASSERT_FALSE(web_lang_parse("deu", &lang));
    TEST_ASSERT_FALSE(web_lang_parse("", &lang));
    TEST_ASSERT_FALSE(web_lang_parse(NULL, &lang));
    TEST_ASSERT_FALSE(web_lang_parse("de", NULL));
    TEST_ASSERT_EQUAL_STRING("de", web_lang_code(WEB_LANG_DE));
    TEST_ASSERT_EQUAL_STRING("en", web_lang_code(WEB_LANG_EN));
}

static void test_lang_from_cookie(void)
{
    web_lang_t lang = WEB_LANG_DE;
    TEST_ASSERT_TRUE(web_lang_from_cookie("lang=en", &lang));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, lang);
    TEST_ASSERT_TRUE(web_lang_from_cookie("a=b; lang=de; c=d", &lang));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, lang);
    TEST_ASSERT_TRUE(web_lang_from_cookie("x=1;lang=en ", &lang));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, lang);
    TEST_ASSERT_FALSE(web_lang_from_cookie("xlang=en", &lang));
    TEST_ASSERT_FALSE(web_lang_from_cookie("lang=fr", &lang));
    TEST_ASSERT_FALSE(web_lang_from_cookie("lang=", &lang));
    TEST_ASSERT_FALSE(web_lang_from_cookie("", &lang));
    TEST_ASSERT_FALSE(web_lang_from_cookie(NULL, &lang));
}

static void test_lang_from_accept(void)
{
    // Ohne Angabe Deutsch
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_from_accept(NULL));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_from_accept(""));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_from_accept(" , "));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_from_accept("en-US,en;q=0.9"));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_from_accept("de-DE,de;q=0.9,en;q=0.8"));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_from_accept("EN-gb"));
    // Höchster q-Wert gewinnt, auch wenn er später steht
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_from_accept("en;q=0.5, de;q=0.8"));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_from_accept("fr-FR,fr;q=0.9,en;q=0.8,de;q=0.7"));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_from_accept("fr, de;q=0.5"));
    // Weder Deutsch noch Englisch gewünscht -> Englisch
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_from_accept("fr"));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_from_accept("*"));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_from_accept("de;q=0"));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_from_accept("de;q=1.0;foo=bar, en;q=0.99"));
}

static void test_lang_select(void)
{
    // Cookie des Schalters vor Browsersprache
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_select("lang=en", "de-DE"));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_select("lang=de", "en-US"));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_select("lang=xx", "en-US"));
    TEST_ASSERT_EQUAL(WEB_LANG_EN, web_lang_select(NULL, "en"));
    TEST_ASSERT_EQUAL(WEB_LANG_DE, web_lang_select(NULL, NULL));
}

static void test_msg_texts(void)
{
    TEST_ASSERT_EQUAL_STRING("Gespeichert. Das Gerät startet neu …",
                             web_msg(WEB_LANG_DE, WEB_MSG_SAVED_RESTART));
    TEST_ASSERT_EQUAL_STRING("Saved. The device is restarting …",
                             web_msg(WEB_LANG_EN, WEB_MSG_SAVED_RESTART));
    TEST_ASSERT_NULL(web_msg(WEB_LANG_DE, WEB_MSG_NONE));
    TEST_ASSERT_NULL(web_msg(WEB_LANG_EN, WEB_MSG_COUNT));
    // Jede Meldung gibt es in beiden Sprachen, und sie unterscheiden sich
    for (int m = WEB_MSG_NONE + 1; m < WEB_MSG_COUNT; m++) {
        const char *de = web_msg(WEB_LANG_DE, (web_msg_t)m);
        const char *en = web_msg(WEB_LANG_EN, (web_msg_t)m);
        TEST_ASSERT_NOT_NULL(de);
        TEST_ASSERT_NOT_NULL(en);
        TEST_ASSERT_NOT_EQUAL(0, strcmp(de, en));
    }
}

static void test_error_json(void)
{
    char buf[128];
    int n = web_error_json("Fehler.", buf, sizeof(buf));
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"Fehler.\"}", buf);
    TEST_ASSERT_EQUAL_INT((int)strlen(buf), n);
    TEST_ASSERT_GREATER_THAN(0, web_error_json("a \"b\" c\\d", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"a \\\"b\\\" c\\\\d\"}", buf);
    TEST_ASSERT_EQUAL_INT(-1, web_error_json(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, web_error_json("a\nb", buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_INT(-1, web_error_json("x", NULL, 10));
    // {"error":"x"} hat 13 Zeichen + '\0'
    TEST_ASSERT_EQUAL_INT(-1, web_error_json("x", buf, 13));
    TEST_ASSERT_EQUAL_INT(13, web_error_json("x", buf, 14));
}

// Vollständige Einstellungen-Seite mit allen Bereichen rendern
static int render_full(char *buf, size_t len, web_lang_t lang)
{
    web_page_info_t info = {
        .lang = lang, .client_mode = true, .ip = "1.2.3.4", .hostname = "h", .ssid = "s", .version = "2026.1",
        .ble_name = "Lampe", .own_mac = "aa:bb:cc:dd:ee:ff", .espnow_peers = "", .espnow_gateway = "",
        .espnow_window_s = 60, .led_gpio_mask = 1ULL << 10, .led_gpio = 10, .led_count = 16,
        .message = "Hinweis",
    };
    return web_page_render(buf, len, &info);
}

static void test_page_english(void)
{
    char buf[8192];
    TEST_ASSERT_GREATER_THAN(0, render_full(buf, sizeof(buf), WEB_LANG_EN));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<html lang=\"en\">"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<h1>Settings</h1>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/\" class=\"cur\">Settings</a>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/colors\">Colors</a>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<dt>Mode</dt><dd>Wi-Fi client</dd>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Save and restart"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "1 to 20 characters"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Own MAC: aa:bb:cc:dd:ee:ff. At most 10 receivers."));
    TEST_ASSERT_NOT_NULL(strstr(buf, "for 60 s;"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "16 to 25 LEDs."));
    // Keine deutschen Texte der Seite
    TEST_ASSERT_NULL(strstr(buf, "Speichern"));
    TEST_ASSERT_NULL(strstr(buf, "Einstellungen"));
}

static void test_page_german_default(void)
{
    char buf[8192];
    TEST_ASSERT_GREATER_THAN(0, render_full(buf, sizeof(buf), WEB_LANG_DE));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<html lang=\"de\">"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<h1>Einstellungen</h1>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "1 bis 20 Zeichen, keine Umlaute."));
    TEST_ASSERT_NOT_NULL(strstr(buf, "Eigene MAC: aa:bb:cc:dd:ee:ff. Höchstens 10 Empfänger."));
    TEST_ASSERT_NOT_NULL(strstr(buf, "16 bis 25 LEDs."));
    TEST_ASSERT_NULL(strstr(buf, "Save and restart"));
}

static void test_page_lang_switch(void)
{
    char buf[8192];
    // Deutsch markiert, Englisch setzt das Cookie und lädt die Seite neu
    TEST_ASSERT_GREATER_THAN(0, render_full(buf, sizeof(buf), WEB_LANG_DE));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<nav class=\"lang\"><a class=\"cur\" title=\"Deutsch\">DE</a>"
                                     "<a href=\"/\" title=\"English\" onclick=\"document.cookie="
                                     "'lang=en;path=/;max-age=31536000;samesite=lax'\">EN</a></nav>"));
    TEST_ASSERT_GREATER_THAN(0, render_full(buf, sizeof(buf), WEB_LANG_EN));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/\" title=\"Deutsch\" onclick=\"document.cookie="
                                     "'lang=de;path=/;max-age=31536000;samesite=lax'\">DE</a>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a class=\"cur\" title=\"English\">EN</a>"));
    // Auf der Farben-Seite führt der Schalter zurück auf /colors
    TEST_ASSERT_GREATER_THAN(0, web_colors_page_render_lang(buf, sizeof(buf), WEB_LANG_DE));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/colors\" title=\"English\""));
}

static void test_colors_page_english(void)
{
    char buf[4096];
    int n = web_colors_page_render_lang(buf, sizeof(buf), WEB_LANG_EN);
    TEST_ASSERT_GREATER_THAN(0, n);
    // Reserve wie bei der deutschen Seite: höchstens 3/4 des Puffers
    TEST_ASSERT_LESS_THAN(3 * 4096 / 4, n);
    TEST_ASSERT_NOT_NULL(strstr(buf, "<html lang=\"en\">"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<h1>Colors</h1>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "<a href=\"/colors\" class=\"cur\">Colors</a>"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "At most 16 colors."));
    TEST_ASSERT_NOT_NULL(strstr(buf, "msg('At most '+MAX+' colors.',1)"));
    TEST_ASSERT_NOT_NULL(strstr(buf, "msg('Saved.')"));
    TEST_ASSERT_NULL(strstr(buf, "Speichern"));
    TEST_ASSERT_EQUAL_INT(-1, web_colors_page_render_lang(buf, 200, WEB_LANG_EN));
    TEST_ASSERT_EQUAL_INT(-1, web_colors_page_render_lang(NULL, 10, WEB_LANG_EN));
}

static void test_server_uses_cookie(void)
{
    // Cookie des Schalters schlägt die Browsersprache
    start_server("theme=x; lang=en", "de-DE,de;q=0.9");
    request(s_get_root, "");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<h1>Settings</h1>"));
    stop_server();

    start_server("lang=de", "en-US");
    request(s_get_root, "");
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<h1>Einstellungen</h1>"));
    stop_server();
}

static void test_server_uses_accept_language(void)
{
    start_server(NULL, "en-US,en;q=0.9,de;q=0.8");
    request(s_get_root, "");
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<h1>Settings</h1>"));
    request(s_get_colors, "");
    TEST_ASSERT_EQUAL_STRING("200 OK", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<h1>Colors</h1>"));
    stop_server();

    start_server(NULL, NULL);
    request(s_get_root, "");
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<h1>Einstellungen</h1>"));
    stop_server();
}

static void test_server_long_accept_language(void)
{
    // Längere Header werden abgeschnitten, der Anfang reicht
    start_server(NULL, "en-US,en;q=0.9,fr-FR;q=0.8,fr;q=0.7,it-IT;q=0.6,it;q=0.5,es-ES;q=0.4,"
                       "es;q=0.3,nl-NL;q=0.2,nl;q=0.1,pt-PT;q=0.1,pt;q=0.1,pl-PL;q=0.1,pl;q=0.1");
    request(s_get_root, "");
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<h1>Settings</h1>"));
    stop_server();
}

static void test_server_messages_english(void)
{
    start_server(NULL, "en");
    request(s_post_config, "ssid=Heim&hostname=bad_host");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_NOT_NULL(strstr(s_sent, "<p class=\"msg err\">Hostname: 1 to 32 characters"));

    request(s_put_colors, "{\"colors\":[\"rot\"]}");
    TEST_ASSERT_EQUAL_STRING("400 Bad Request", s_status);
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"Expected {\\\"colors\\\":[\\\"#rrggbb\\\", ...]} "
                             "with at most 16 colors.\"}", s_sent);
    stop_server();
}

static void test_server_messages_german(void)
{
    start_server(NULL, NULL);
    request(s_put_colors, "{\"colors\":[\"rot\"]}");
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"Erwartet {\\\"colors\\\":[\\\"#rrggbb\\\", ...]} "
                             "mit höchstens 16 Farben.\"}", s_sent);
    stop_server();
}

// Wird von test_main.c aufgerufen
void run_web_lang_tests(void)
{
    RUN_TEST(test_lang_parse_and_code);
    RUN_TEST(test_lang_from_cookie);
    RUN_TEST(test_lang_from_accept);
    RUN_TEST(test_lang_select);
    RUN_TEST(test_msg_texts);
    RUN_TEST(test_error_json);
    RUN_TEST(test_page_english);
    RUN_TEST(test_page_german_default);
    RUN_TEST(test_page_lang_switch);
    RUN_TEST(test_colors_page_english);
    RUN_TEST(test_server_uses_cookie);
    RUN_TEST(test_server_uses_accept_language);
    RUN_TEST(test_server_long_accept_language);
    RUN_TEST(test_server_messages_english);
    RUN_TEST(test_server_messages_german);
}
