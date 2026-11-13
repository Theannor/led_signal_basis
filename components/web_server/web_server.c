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
#define WEB_URI_MAX    12    // Platz für Seiten-Handler
#define WEB_JSON_MAX   256   // reicht für LED_PALETTE_MAX Farben
#define WEB_CSS_CACHE  "max-age=3600"  // Stylesheet eine Stunde im Browser-Cache

// Zahl als Text in String-Literalen
#define STR_(x) #x
#define STR(x)  STR_(x)

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

// Seite mit optionalem Hinweis senden
static esp_err_t send_page(httpd_req_t *req, const char *status, const char *message, bool is_error)
{
    char ip[16] = "-";
    if (s_ctx.get_ip != NULL) {
        s_ctx.get_ip(ip, sizeof(ip));
    }
    // ESP-NOW-Bereich nur mit bekannter eigener MAC
    bool espnow = espnow_mac_is_set(s_ctx.own_mac);
    char own_mac[ESPNOW_MAC_STR_LEN] = "";
    char gateway[ESPNOW_MAC_STR_LEN] = "";
    char peers[ESPNOW_PEER_LIST_STR_MAX] = "";
    if (espnow) {
        espnow_mac_format(s_ctx.own_mac, own_mac);
        if (espnow_mac_is_set(s_ctx.espnow.gateway)) {
            espnow_mac_format(s_ctx.espnow.gateway, gateway);
        }
        espnow_peer_list_format(&s_ctx.espnow, peers, sizeof(peers));
    }
    web_page_info_t info = {
        .client_mode = s_ctx.client_mode,
        .ip = ip,
        .hostname = s_ctx.config.hostname,
        .ssid = s_ctx.config.ssid,
        .ble_name = s_ctx.ble_name[0] != '\0' ? s_ctx.ble_name : NULL,
        .message = message,
        .message_is_error = is_error,
        .receiver_mode = s_ctx.receiver_mode,
        .own_mac = espnow ? own_mac : NULL,
        .espnow_receiver = s_ctx.espnow.role == ESPNOW_ROLE_RECEIVER,
        .espnow_peers = peers,
        .espnow_gateway = gateway,
        .espnow_window_s = s_ctx.espnow_window_s,
        .led_gpio_mask = s_ctx.led_gpio_mask,
        .led_gpio = s_ctx.led.gpio,
        .led_count = s_ctx.led.count,
    };

    char *page = malloc(WEB_PAGE_MAX);
    if (page == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int len = web_page_render(page, WEB_PAGE_MAX, &info);
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

static esp_err_t handle_get_root(httpd_req_t *req)
{
    return send_page(req, "200 OK", NULL, false);
}

// Hinweis für Empfänger: Farben kommen nur per ESP-NOW
#define RECEIVER_TEXT  "Dieses Gerät ist ESP-NOW-Empfänger und übernimmt die Farben vom Gateway."

static esp_err_t handle_get_colors_page(httpd_req_t *req)
{
    if (s_ctx.receiver_mode) {
        return send_page(req, "403 Forbidden", RECEIVER_TEXT, true);
    }
    char *page = malloc(WEB_PAGE_MAX);
    if (page == NULL) {
        return ESP_ERR_NO_MEM;
    }
    int len = web_colors_page_render(page, WEB_PAGE_MAX);
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

// Aktuelle Palette als {"colors":[...]} senden
static esp_err_t send_palette(httpd_req_t *req)
{
    char json[WEB_JSON_MAX];
    led_palette_t palette;
    palette_get(&palette);
    if (led_palette_to_json(&palette, json, sizeof(json)) < 0) {
        ESP_LOGE(TAG, "Palette passt nicht in den JSON-Puffer");
        return send_json(req, "500 Internal Server Error", "{\"error\":\"Interner Fehler.\"}");
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
static const char *error_text(net_config_result_t result)
{
    switch (result) {
    case NET_CONFIG_ERR_SSID:     return "SSID ist zu lang (max. 32 Zeichen).";
    case NET_CONFIG_ERR_PASSWORD: return "Passwort muss 8 bis 63 Zeichen haben.";
    case NET_CONFIG_ERR_HOSTNAME: return "Hostname: 1 bis 32 Zeichen, nur a-z, 0-9 und '-', nicht am Anfang/Ende.";
    default:                      return "Ungültige Eingabe.";
    }
}

static esp_err_t handle_post_config(httpd_req_t *req)
{
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", "Anfrage zu groß oder unvollständig.", true);
    }

    // Neue Werte auf Basis der gespeicherten Konfiguration
    net_config_t cfg = s_ctx.config;
    char password[NET_CONFIG_PASSWORD_MAX + 1];
    char open_flag[4];
    if (!web_form_get_field(body, "ssid", cfg.ssid, sizeof(cfg.ssid))) {
        return send_page(req, "400 Bad Request", error_text(NET_CONFIG_ERR_SSID), true);
    }
    if (!web_form_get_field(body, "hostname", cfg.hostname, sizeof(cfg.hostname))) {
        return send_page(req, "400 Bad Request", error_text(NET_CONFIG_ERR_HOSTNAME), true);
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
        return send_page(req, "400 Bad Request", error_text(NET_CONFIG_ERR_PASSWORD), true);
    }

    net_config_result_t result = net_config_validate(&cfg);
    if (result != NET_CONFIG_OK) {
        return send_page(req, "400 Bad Request", error_text(result), true);
    }

    esp_err_t err = config_store_save(&cfg);
    if (err != ESP_OK) {
        return send_page(req, "500 Internal Server Error", "Speichern fehlgeschlagen.", true);
    }
    s_ctx.config = cfg;
    err = send_page(req, "200 OK", "Gespeichert. Das Gerät startet neu …", false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

static esp_err_t handle_post_ble(httpd_req_t *req)
{
    static const char *const invalid = "Bluetooth-Name: 1 bis " STR(NET_CONFIG_BLE_NAME_MAX)
                                       " Zeichen, ohne Umlaute.";
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", "Anfrage zu groß oder unvollständig.", true);
    }
    char name[NET_CONFIG_BLE_NAME_MAX + 1];
    if (!web_form_get_field(body, "ble_name", name, sizeof(name))) {
        return send_page(req, "400 Bad Request", invalid, true);
    }
    web_form_trim(name);
    if (!net_config_ble_name_is_valid(name)) {
        return send_page(req, "400 Bad Request", invalid, true);
    }
    if (ble_name_store_save(name) != ESP_OK) {
        return send_page(req, "500 Internal Server Error", "Speichern fehlgeschlagen.", true);
    }
    strlcpy(s_ctx.ble_name, name, sizeof(s_ctx.ble_name));
    esp_err_t err = send_page(req, "200 OK", "Gespeichert. Das Gerät startet neu …", false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

// Fehlertext zu ungültigen ESP-NOW-Einstellungen
static const char *espnow_error_text(espnow_config_result_t result)
{
    switch (result) {
    case ESPNOW_CONFIG_ERR_PEERS:
        return "Empfänger: bis zu " STR(ESPNOW_PEERS_MAX) " MAC-Adressen wie aa:bb:cc:dd:ee:ff, "
               "mit Komma getrennt, nicht die eigene.";
    case ESPNOW_CONFIG_ERR_GATEWAY:
        return "Gateway: eine MAC-Adresse wie aa:bb:cc:dd:ee:ff (nicht die eigene) angeben.";
    default:
        return "Bitte Gateway oder Empfänger wählen.";
    }
}

static esp_err_t handle_post_espnow(httpd_req_t *req)
{
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", "Anfrage zu groß oder unvollständig.", true);
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
        return send_page(req, "400 Bad Request", espnow_error_text(ESPNOW_CONFIG_ERR_ROLE), true);
    }

    // Fehlende Felder gelten als leer, zu lange als Fehler
    char peers[ESPNOW_PEER_LIST_STR_MAX + 32];
    if (!web_form_get_field(body, "peers", peers, sizeof(peers))) {
        if (strstr(body, "peers=") != NULL) {
            return send_page(req, "400 Bad Request", espnow_error_text(ESPNOW_CONFIG_ERR_PEERS), true);
        }
        peers[0] = '\0';
    }
    if (!espnow_peer_list_parse(peers, &cfg)) {
        return send_page(req, "400 Bad Request", espnow_error_text(ESPNOW_CONFIG_ERR_PEERS), true);
    }
    char gateway[ESPNOW_MAC_STR_LEN + 16];
    if (!web_form_get_field(body, "gateway", gateway, sizeof(gateway))) {
        if (strstr(body, "gateway=") != NULL) {
            return send_page(req, "400 Bad Request", espnow_error_text(ESPNOW_CONFIG_ERR_GATEWAY), true);
        }
        gateway[0] = '\0';
    }
    web_form_trim(gateway);
    if (gateway[0] == '\0') {
        memset(cfg.gateway, 0, sizeof(cfg.gateway));
    } else if (!espnow_mac_parse(gateway, cfg.gateway)) {
        return send_page(req, "400 Bad Request", espnow_error_text(ESPNOW_CONFIG_ERR_GATEWAY), true);
    }

    espnow_config_result_t result = espnow_config_validate(&cfg, s_ctx.own_mac);
    if (result != ESPNOW_CONFIG_OK) {
        return send_page(req, "400 Bad Request", espnow_error_text(result), true);
    }
    if (espnow_store_save(&cfg) != ESP_OK) {
        return send_page(req, "500 Internal Server Error", "Speichern fehlgeschlagen.", true);
    }
    s_ctx.espnow = cfg;
    esp_err_t err = send_page(req, "200 OK", "Gespeichert. Das Gerät startet neu …", false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

static esp_err_t handle_post_led(httpd_req_t *req)
{
    static const char *const invalid = "LED-Strip: GPIO aus der Liste wählen, "
                                       STR(LED_CONFIG_COUNT_MIN) " bis " STR(LED_CONFIG_COUNT_MAX)
                                       " LEDs.";
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_page(req, "400 Bad Request", "Anfrage zu groß oder unvollständig.", true);
    }
    char gpio_text[8];
    char count_text[8];
    unsigned gpio;
    unsigned count;
    if (!web_form_get_field(body, "led_gpio", gpio_text, sizeof(gpio_text)) ||
        !web_form_get_field(body, "led_count", count_text, sizeof(count_text))) {
        return send_page(req, "400 Bad Request", invalid, true);
    }
    web_form_trim(count_text);
    if (!led_config_parse_uint(gpio_text, UINT8_MAX, &gpio) ||
        !led_config_parse_uint(count_text, UINT8_MAX, &count)) {
        return send_page(req, "400 Bad Request", invalid, true);
    }
    led_config_t cfg = {.gpio = (uint8_t)gpio, .count = (uint8_t)count};
    if (!led_config_is_valid(&cfg, s_ctx.led_gpio_mask)) {
        return send_page(req, "400 Bad Request", invalid, true);
    }
    if (led_store_save(&cfg, s_ctx.led_gpio_mask) != ESP_OK) {
        return send_page(req, "500 Internal Server Error", "Speichern fehlgeschlagen.", true);
    }
    s_ctx.led = cfg;
    esp_err_t err = send_page(req, "200 OK", "Gespeichert. Das Gerät startet neu …", false);
    if (s_ctx.schedule_restart != NULL) {
        s_ctx.schedule_restart();
    }
    return err;
}

static esp_err_t handle_put_api_colors(httpd_req_t *req)
{
    if (s_ctx.receiver_mode) {
        return send_json(req, "403 Forbidden", "{\"error\":\"" RECEIVER_TEXT "\"}");
    }
    char body[WEB_BODY_MAX];
    if (!read_body(req, body, sizeof(body))) {
        return send_json(req, "400 Bad Request",
                         "{\"error\":\"Anfrage zu groß oder unvollständig.\"}");
    }
    led_palette_t palette;
    if (!led_palette_from_json(body, &palette)) {
        return send_json(req, "400 Bad Request",
                         "{\"error\":\"Erwartet {\\\"colors\\\":[\\\"#rrggbb\\\", ...]} "
                         "mit höchstens " STR(LED_PALETTE_MAX) " Farben.\"}");
    }
    if (palette_store_save(&palette) != ESP_OK) {
        return send_json(req, "500 Internal Server Error",
                         "{\"error\":\"Speichern fehlgeschlagen.\"}");
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
        {.uri = "/style.css", .method = HTTP_GET, .handler = handle_get_style},
        {.uri = "/api/colors", .method = HTTP_GET, .handler = handle_get_api_colors},
        {.uri = "/api/colors", .method = HTTP_PUT, .handler = handle_put_api_colors},
        {.uri = "/ble", .method = HTTP_POST, .handler = handle_post_ble},
        {.uri = "/espnow", .method = HTTP_POST, .handler = handle_post_espnow},
        {.uri = "/led", .method = HTTP_POST, .handler = handle_post_led},
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
