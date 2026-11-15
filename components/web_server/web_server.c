// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <stdlib.h>
#include <string.h>
#include "web_server.h"
#include "ble_name_store.h"
#include "config_store.h"
#include "espnow_store.h"
#include "led_store.h"
#include "palette_store.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "web_form.h"

// Grenzen für Anfrage und Antwort
#define WEB_BODY_MAX   768   // ESP-NOW-Liste: bis zu 10 URL-kodierte MAC-Adressen
#define WEB_PAGE_MAX   6144
#define WEB_URI_MAX    16    // Platz für Seiten-Handler
#define WEB_JSON_MAX   256   // reicht für LED_PALETTE_MAX Farben
#define WEB_VERSION_UNKNOWN  "0+unknown"   // wenn ctx.version fehlt
#define WEB_CSS_CACHE  "max-age=3600"  // Stylesheet eine Stunde im Browser-Cache
#define WEB_HDR_MAX    128   // gelesene Länge von Cookie und Accept-Language (Rest wird abgeschnitten)

static const char *TAG = "web_server";

static web_server_ctx_t s_ctx;
static httpd_handle_t s_server = NULL;
// Schützt s_ctx.palette: wird auch aus der Bluetooth-Task geändert
static SemaphoreHandle_t s_palette_lock = NULL;

static void palette_get(led_palette_t *out)
{
    xSemaphoreTake(s_palette_lock, portMAX_DELAY);
    *out = s_ctx.palette;
    xSemaphoreGive(s_palette_lock);
}

static void palette_set(const led_palette_t *palette)
{
    xSemaphoreTake(s_palette_lock, portMAX_DELAY);
    s_ctx.palette = *palette;
    xSemaphoreGive(s_palette_lock);
}

// Header-Wert lesen; zu lange Werte abgeschnitten, fehlende als ""
static void read_header(httpd_req_t *req, const char *field, char *buf, size_t len)
{
    buf[0] = '\0';
    esp_err_t err = httpd_req_get_hdr_value_str(req, field, buf, len);
    if (err != ESP_OK && err != ESP_ERR_HTTPD_RESULT_TRUNC) {
        buf[0] = '\0';
    }
}

// Sprache der Anfrage: Cookie des Sprachschalters, sonst Accept-Language des Browsers
static web_lang_t request_lang(httpd_req_t *req)
{
    char cookie[WEB_HDR_MAX];
    char accept[WEB_HDR_MAX];
    read_header(req, "Cookie", cookie, sizeof(cookie));
    read_header(req, "Accept-Language", accept, sizeof(accept));
    return web_lang_select(cookie, accept[0] != '\0' ? accept : NULL);
}

// Puffer für die Texte in web_page_info_t
typedef struct {
    char ip[16];
    char own_mac[ESPNOW_MAC_STR_LEN];
    char gateway[ESPNOW_MAC_STR_LEN];
    char peers[ESPNOW_PEER_LIST_STR_MAX];
} page_bufs_t;

// Seitendaten aus dem aktuellen Zustand in der Sprache der Anfrage; Texte zeigen in bufs
static void fill_page_info(httpd_req_t *req, web_page_info_t *info, page_bufs_t *bufs)
{
    memset(bufs, 0, sizeof(*bufs));
    strlcpy(bufs->ip, "-", sizeof(bufs->ip));
    if (s_ctx.get_ip != NULL) {
        s_ctx.get_ip(bufs->ip, sizeof(bufs->ip));
    }
    // ESP-NOW-Bereich nur mit bekannter eigener MAC
    bool espnow = espnow_mac_is_set(s_ctx.own_mac);
    if (espnow) {
        espnow_mac_format(s_ctx.own_mac, bufs->own_mac);
        if (espnow_mac_is_set(s_ctx.espnow.gateway)) {
            espnow_mac_format(s_ctx.espnow.gateway, bufs->gateway);
        }
        espnow_peer_list_format(&s_ctx.espnow, bufs->peers, sizeof(bufs->peers));
    }
    *info = (web_page_info_t){
        .lang = request_lang(req),
        .client_mode = s_ctx.client_mode,
        .ip = bufs->ip,
        .hostname = s_ctx.config.hostname,
        .version = s_ctx.version,
        .ssid = s_ctx.config.ssid,
        .ble_name = s_ctx.ble_name[0] != '\0' ? s_ctx.ble_name : NULL,
        .receiver_mode = s_ctx.receiver_mode,
        .own_mac = espnow ? bufs->own_mac : NULL,
        .espnow_receiver = s_ctx.espnow.role == ESPNOW_ROLE_RECEIVER,
        .espnow_peers = bufs->peers,
        .espnow_gateway = bufs->gateway,
        .espnow_window_s = s_ctx.espnow_window_s,
        .led_gpio_mask = s_ctx.led_gpio_mask,
        .led_gpio = s_ctx.led.gpio,
        .led_count = s_ctx.led.count,
    };
}

// HTML-Seite mit render bauen und senden
static esp_err_t send_rendered(httpd_req_t *req, const char *status,
                               int (*render)(char *, size_t, const web_page_info_t *),
                               const web_page_info_t *info)
{
    char *page = malloc(WEB_PAGE_MAX);
    if (page == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int len = render(page, WEB_PAGE_MAX, info);
    esp_err_t err;
    if (len < 0) {
        ESP_LOGE(TAG, "Seite passt nicht in den Puffer");
        err = ESP_FAIL;
    } else {
        httpd_resp_set_status(req, status);
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        err = httpd_resp_send(req, page, len);
    }
    free(page);
    return err;
}

// Unterseite der Einstellungen mit optionalem Hinweis in der Sprache der Anfrage senden
static esp_err_t send_page(httpd_req_t *req, const char *status, web_settings_page_t page,
                           web_msg_t msg, bool is_error)
{
    web_page_info_t info;
    page_bufs_t bufs;
    fill_page_info(req, &info, &bufs);
    info.settings_page = page;
    info.message = web_msg(info.lang, msg);
    info.message_is_error = is_error;
    return send_rendered(req, status, web_page_render, &info);
}

// Status-Seite, auch im Empfänger-Modus
static esp_err_t handle_get_status_page(httpd_req_t *req)
{
    web_page_info_t info;
    page_bufs_t bufs;
    fill_page_info(req, &info, &bufs);
    return send_rendered(req, "200 OK", web_status_page_render, &info);
}

// Unterseiten der Einstellungen: WLAN (/), Bluetooth, ESP-NOW, LED-Strip
static esp_err_t handle_get_root(httpd_req_t *req)
{
    return send_page(req, "200 OK", WEB_SETTINGS_WIFI, WEB_MSG_NONE, false);
}

static esp_err_t handle_get_ble_page(httpd_req_t *req)
{
    return send_page(req, "200 OK", WEB_SETTINGS_BLE, WEB_MSG_NONE, false);
}

static esp_err_t handle_get_espnow_page(httpd_req_t *req)
{
    return send_page(req, "200 OK", WEB_SETTINGS_ESPNOW, WEB_MSG_NONE, false);
}

static esp_err_t handle_get_led_page(httpd_req_t *req)
{
    return send_page(req, "200 OK", WEB_SETTINGS_LED, WEB_MSG_NONE, false);
}

static esp_err_t handle_get_colors_page(httpd_req_t *req)
{
    if (s_ctx.receiver_mode) {
        return send_page(req, "403 Forbidden", WEB_SETTINGS_WIFI, WEB_MSG_RECEIVER, true);
    }
    web_lang_t lang = request_lang(req);
    char *page = malloc(WEB_PAGE_MAX);
    if (page == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int len = web_colors_page_render_lang(page, WEB_PAGE_MAX, lang);
    esp_err_t err;
    if (len < 0) {
        ESP_LOGE(TAG, "Farben-Seite passt nicht in den Puffer");
        err = ESP_FAIL;
    } else {
        httpd_resp_set_status(req, "200 OK");
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        err = httpd_resp_send(req, page, len);
    }
    free(page);
    return err;
}

// Stylesheet aus dem Flash senden; der Browser darf es zwischenspeichern
static esp_err_t handle_get_style(httpd_req_t *req)
{
    const char *css = web_style_css();
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Cache-Control", WEB_CSS_CACHE);
    return httpd_resp_send(req, css, (ssize_t)strlen(css));
}

// JSON-Antwort senden
static esp_err_t send_json(httpd_req_t *req, const char *status, const char *json)
{
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, json, (ssize_t)strlen(json));
}

// Fehler als {"error":"..."} in der Sprache der Anfrage senden
static esp_err_t send_json_error(httpd_req_t *req, const char *status, web_msg_t msg)
{
    char json[WEB_JSON_MAX];
    if (web_error_json(web_msg(request_lang(req), msg), json, sizeof(json)) < 0) {
        ESP_LOGE(TAG, "Fehlermeldung passt nicht ins JSON");
        return send_json(req, status, "{\"error\":\"\"}");
    }
    return send_json(req, status, json);
}

// Firmware-Version als {"version":"..."} senden
static esp_err_t handle_get_api_version(httpd_req_t *req)
{
    char json[WEB_JSON_MAX];
    const char *version = s_ctx.version != NULL ? s_ctx.version : WEB_VERSION_UNKNOWN;
    if (web_version_json(version, json, sizeof(json)) < 0) {
        ESP_LOGE(TAG, "Version passt nicht ins JSON");
        return send_json_error(req, "500 Internal Server Error", WEB_MSG_VERSION_INVALID);
    }
    return send_json(req, "200 OK", json);
}

// Aktuelle Palette als {"colors":[...]} senden
static esp_err_t send_palette(httpd_req_t *req)
{
    char json[WEB_JSON_MAX];
    led_palette_t palette;
    palette_get(&palette);
    if (led_palette_to_json(&palette, json, sizeof(json)) < 0) {
        ESP_LOGE(TAG, "Palette passt nicht in den JSON-Puffer");
        return send_json_error(req, "500 Internal Server Error", WEB_MSG_INTERNAL);
    }
    return send_json(req, "200 OK", json);
}

static esp_err_t handle_get_api_colors(httpd_req_t *req)
{
    return send_palette(req);
}

// Ganzen Body lesen; false bei zu großem oder abgebrochenem Body
static bool read_body(httpd_req_t *req, char *body, size_t body_len)
{
    if (req->content_len >= body_len) {
        return false;
    }
    size_t received = 0;
    while (received < req->content_len) {
        int n = httpd_req_recv(req, body + received, req->content_len - received);
        if (n <= 0) {
            return false;
        }
        received += (size_t)n;
    }
    body[received] = '\0';
    return true;
}

// Fehlertext zu einem ungültigen Feld
static web_msg_t error_text(net_config_result_t result)
{
    switch (result) {
    case NET_CONFIG_ERR_SSID:     return WEB_MSG_SSID;
    case NET_CONFIG_ERR_PASSWORD: return WEB_MSG_PASSWORD;
    case NET_CONFIG_ERR_HOSTNAME: return WEB_MSG_HOSTNAME;
    default:                      return WEB_MSG_INVALID;
    }
}

static esp_err_t handle_post_config(httpd_req_t *req)
{
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_WIFI, WEB_MSG_BAD_REQUEST, true);
    }

    // Neue Werte auf Basis der gespeicherten Konfiguration
    net_config_t cfg = s_ctx.config;
    char password[NET_CONFIG_PASSWORD_MAX + 1];
    char open_flag[4];
    if (!web_form_get_field(body, "ssid", cfg.ssid, sizeof(cfg.ssid))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_WIFI, error_text(NET_CONFIG_ERR_SSID), true);
    }
    if (!web_form_get_field(body, "hostname", cfg.hostname, sizeof(cfg.hostname))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_WIFI, error_text(NET_CONFIG_ERR_HOSTNAME), true);
    }
    // Handy-Tastaturen hängen gern ein Leerzeichen an -> entfernen (nicht beim Passwort)
    web_form_trim(cfg.ssid);
    web_form_trim(cfg.hostname);
    if (web_form_get_field(body, "open", open_flag, sizeof(open_flag))) {
        cfg.password[0] = '\0';  // offenes WLAN
    } else if (web_form_get_field(body, "password", password, sizeof(password))) {
        if (password[0] != '\0') {
            strlcpy(cfg.password, password, sizeof(cfg.password));
        }
        // leer = bisheriges Passwort behalten
    } else if (strstr(body, "password=") != NULL) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_WIFI, error_text(NET_CONFIG_ERR_PASSWORD), true);
    }

    net_config_result_t result = net_config_validate(&cfg);
    if (result != NET_CONFIG_OK) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_WIFI, error_text(result), true);
    }

    esp_err_t err = config_store_save(&cfg);
    if (err != ESP_OK) {
        return send_page(req, "500 Internal Server Error", WEB_SETTINGS_WIFI, WEB_MSG_SAVE_FAILED, true);
    }
    s_ctx.config = cfg;
    err = send_page(req, "200 OK", WEB_SETTINGS_WIFI, WEB_MSG_SAVED_RESTART, false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

static esp_err_t handle_post_ble(httpd_req_t *req)
{
    const web_msg_t invalid = WEB_MSG_BLE_NAME;
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_BLE, WEB_MSG_BAD_REQUEST, true);
    }
    char name[NET_CONFIG_BLE_NAME_MAX + 1];
    if (!web_form_get_field(body, "ble_name", name, sizeof(name))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_BLE, invalid, true);
    }
    web_form_trim(name);
    if (!net_config_ble_name_is_valid(name)) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_BLE, invalid, true);
    }
    if (ble_name_store_save(name) != ESP_OK) {
        return send_page(req, "500 Internal Server Error", WEB_SETTINGS_BLE, WEB_MSG_SAVE_FAILED, true);
    }
    strlcpy(s_ctx.ble_name, name, sizeof(s_ctx.ble_name));
    esp_err_t err = send_page(req, "200 OK", WEB_SETTINGS_BLE, WEB_MSG_SAVED_RESTART, false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

// Fehlertext zu ungültigen ESP-NOW-Einstellungen
static web_msg_t espnow_error_text(espnow_config_result_t result)
{
    switch (result) {
    case ESPNOW_CONFIG_ERR_PEERS:   return WEB_MSG_ESPNOW_PEERS;
    case ESPNOW_CONFIG_ERR_GATEWAY: return WEB_MSG_ESPNOW_GATEWAY;
    default:                        return WEB_MSG_ESPNOW_ROLE;
    }
}

static esp_err_t handle_post_espnow(httpd_req_t *req)
{
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_ESPNOW, WEB_MSG_BAD_REQUEST, true);
    }
    espnow_config_t cfg = s_ctx.espnow;
    char role[12];
    if (!web_form_get_field(body, "espnow_role", role, sizeof(role))) {
        role[0] = '\0';
    }
    if (strcmp(role, "gateway") == 0) {
        cfg.role = ESPNOW_ROLE_GATEWAY;
    } else if (strcmp(role, "receiver") == 0) {
        cfg.role = ESPNOW_ROLE_RECEIVER;
    } else {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_ESPNOW, espnow_error_text(ESPNOW_CONFIG_ERR_ROLE), true);
    }

    // Fehlende Felder gelten als leer, zu lange als Fehler
    char peers[ESPNOW_PEER_LIST_STR_MAX + 32];
    if (!web_form_get_field(body, "peers", peers, sizeof(peers))) {
        if (strstr(body, "peers=") != NULL) {
            return send_page(req, "400 Bad Request", WEB_SETTINGS_ESPNOW, espnow_error_text(ESPNOW_CONFIG_ERR_PEERS), true);
        }
        peers[0] = '\0';
    }
    if (!espnow_peer_list_parse(peers, &cfg)) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_ESPNOW, espnow_error_text(ESPNOW_CONFIG_ERR_PEERS), true);
    }
    char gateway[ESPNOW_MAC_STR_LEN + 16];
    if (!web_form_get_field(body, "gateway", gateway, sizeof(gateway))) {
        if (strstr(body, "gateway=") != NULL) {
            return send_page(req, "400 Bad Request", WEB_SETTINGS_ESPNOW, espnow_error_text(ESPNOW_CONFIG_ERR_GATEWAY), true);
        }
        gateway[0] = '\0';
    }
    web_form_trim(gateway);
    if (gateway[0] == '\0') {
        memset(cfg.gateway, 0, sizeof(cfg.gateway));
    } else if (!espnow_mac_parse(gateway, cfg.gateway)) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_ESPNOW, espnow_error_text(ESPNOW_CONFIG_ERR_GATEWAY), true);
    }

    espnow_config_result_t result = espnow_config_validate(&cfg, s_ctx.own_mac);
    if (result != ESPNOW_CONFIG_OK) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_ESPNOW, espnow_error_text(result), true);
    }
    if (espnow_store_save(&cfg) != ESP_OK) {
        return send_page(req, "500 Internal Server Error", WEB_SETTINGS_ESPNOW, WEB_MSG_SAVE_FAILED, true);
    }
    s_ctx.espnow = cfg;
    esp_err_t err = send_page(req, "200 OK", WEB_SETTINGS_ESPNOW, WEB_MSG_SAVED_RESTART, false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

static esp_err_t handle_post_led(httpd_req_t *req)
{
    const web_msg_t invalid = WEB_MSG_LED;
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_LED, WEB_MSG_BAD_REQUEST, true);
    }
    char gpio_text[8];
    char count_text[8];
    unsigned gpio;
    unsigned count;
    if (!web_form_get_field(body, "led_gpio", gpio_text, sizeof(gpio_text)) ||
        !web_form_get_field(body, "led_count", count_text, sizeof(count_text))) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_LED, invalid, true);
    }
    web_form_trim(count_text);
    if (!led_config_parse_uint(gpio_text, UINT8_MAX, &gpio) ||
        !led_config_parse_uint(count_text, UINT8_MAX, &count)) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_LED, invalid, true);
    }
    led_config_t cfg = {.gpio = (uint8_t)gpio, .count = (uint8_t)count};
    if (!led_config_is_valid(&cfg, s_ctx.led_gpio_mask)) {
        return send_page(req, "400 Bad Request", WEB_SETTINGS_LED, invalid, true);
    }
    if (led_store_save(&cfg, s_ctx.led_gpio_mask) != ESP_OK) {
        return send_page(req, "500 Internal Server Error", WEB_SETTINGS_LED, WEB_MSG_SAVE_FAILED, true);
    }
    s_ctx.led = cfg;
    esp_err_t err = send_page(req, "200 OK", WEB_SETTINGS_LED, WEB_MSG_SAVED_RESTART, false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

static esp_err_t handle_put_api_colors(httpd_req_t *req)
{
    if (s_ctx.receiver_mode) {
        return send_json_error(req, "403 Forbidden", WEB_MSG_RECEIVER);
    }
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_json_error(req, "400 Bad Request", WEB_MSG_BAD_REQUEST);
    }
    led_palette_t palette;
    if (!led_palette_from_json(body, &palette)) {
        return send_json_error(req, "400 Bad Request", WEB_MSG_PALETTE_FORMAT);
    }
    if (palette_store_save(&palette) != ESP_OK) {
        return send_json_error(req, "500 Internal Server Error", WEB_MSG_SAVE_FAILED);
    }
    palette_set(&palette);
    if (s_ctx.on_palette_changed != NULL) {
        s_ctx.on_palette_changed(&palette);
    }
    return send_palette(req);
}

esp_err_t web_server_start(const web_server_ctx_t *ctx)
{
    if (ctx == NULL || s_server != NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_palette_lock == NULL) {
        s_palette_lock = xSemaphoreCreateMutex();
        if (s_palette_lock == NULL) {
            return ESP_ERR_NO_MEM;
        }
    }
    s_ctx = *ctx;

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = ctx->port;
    config.max_uri_handlers = WEB_URI_MAX;
    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP-Server starten fehlgeschlagen: %s", esp_err_to_name(err));
        s_server = NULL;
        return err;
    }

    const httpd_uri_t uris[] = {
        {.uri = "/", .method = HTTP_GET, .handler = handle_get_root},
        {.uri = "/config", .method = HTTP_POST, .handler = handle_post_config},
        {.uri = "/colors", .method = HTTP_GET, .handler = handle_get_colors_page},
        {.uri = "/status", .method = HTTP_GET, .handler = handle_get_status_page},
        {.uri = "/style.css", .method = HTTP_GET, .handler = handle_get_style},
        {.uri = "/api/colors", .method = HTTP_GET, .handler = handle_get_api_colors},
        {.uri = "/api/colors", .method = HTTP_PUT, .handler = handle_put_api_colors},
        {.uri = "/ble", .method = HTTP_GET, .handler = handle_get_ble_page},
        {.uri = "/ble", .method = HTTP_POST, .handler = handle_post_ble},
        {.uri = "/espnow", .method = HTTP_GET, .handler = handle_get_espnow_page},
        {.uri = "/espnow", .method = HTTP_POST, .handler = handle_post_espnow},
        {.uri = "/led", .method = HTTP_GET, .handler = handle_get_led_page},
        {.uri = "/led", .method = HTTP_POST, .handler = handle_post_led},
        {.uri = "/api/version", .method = HTTP_GET, .handler = handle_get_api_version},
    };
    for (size_t i = 0; i < sizeof(uris) / sizeof(uris[0]) && err == ESP_OK; i++) {
        err = httpd_register_uri_handler(s_server, &uris[i]);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Seiten registrieren fehlgeschlagen: %s", esp_err_to_name(err));
        httpd_stop(s_server);
        s_server = NULL;
        return err;
    }
    ESP_LOGI(TAG, "Webseite auf Port %u bereit", ctx->port);
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (s_server == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = httpd_stop(s_server);
    s_server = NULL;
    return err;
}

esp_err_t web_server_set_palette(const led_palette_t *palette)
{
    if (palette == NULL || palette->count > LED_PALETTE_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_server == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    palette_set(palette);
    return ESP_OK;
}
