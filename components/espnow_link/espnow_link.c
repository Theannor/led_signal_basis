#include <stdatomic.h>
#include <string.h>
#include "espnow_link.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "espnow_proto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define LINK_TASK_STACK  3072
#define LINK_TASK_PRIO   5
#define LINK_QUEUE_LEN   4

// Wert der Task-Benachrichtigung nach einem gesendeten HELLO
#define NOTIFY_ACK   1
#define NOTIFY_FAIL  2

static const char *TAG = "espnow_link";

typedef enum {
    LINK_OFF,
    LINK_GATEWAY,
    LINK_RECEIVER,
} link_mode_t;

// Eintrag der Warteschlange: HELLO (Gateway) bzw. PALETTE (Empfänger)
typedef struct {
    uint8_t src[ESPNOW_MAC_LEN];
    espnow_msg_t msg;
} link_event_t;

static link_mode_t s_mode = LINK_OFF;
static espnow_config_t s_cfg;
static espnow_link_receiver_cfg_t s_rcfg;
static espnow_scan_t s_scan;
static espnow_link_get_palette_t s_get_palette;
static espnow_link_on_palette_t s_on_palette;
static QueueHandle_t s_queue = NULL;
static TaskHandle_t s_task = NULL;
static atomic_bool s_connected = false;

// Läuft im WLAN-Task: nur prüfen und weiterreichen
static void recv_cb(const esp_now_recv_info_t *info, const uint8_t *data, int len)
{
    if (info == NULL || info->src_addr == NULL || data == NULL || len <= 0 || s_queue == NULL) {
        return;
    }
    link_event_t ev;
    if (!espnow_proto_decode(data, (size_t)len, &ev.msg)) {
        return;
    }
    memcpy(ev.src, info->src_addr, ESPNOW_MAC_LEN);
    bool wanted = false;
    if (s_mode == LINK_GATEWAY) {
        wanted = ev.msg.type == ESPNOW_MSG_HELLO && espnow_config_has_peer(&s_cfg, ev.src);
    } else if (s_mode == LINK_RECEIVER) {
        wanted = ev.msg.type == ESPNOW_MSG_PALETTE &&
                 memcmp(ev.src, s_cfg.gateway, ESPNOW_MAC_LEN) == 0;
    }
    if (wanted && xQueueSend(s_queue, &ev, 0) != pdTRUE) {
        ESP_LOGW(TAG, "Warteschlange voll, Paket von " MACSTR " verworfen", MAC2STR(ev.src));
    }
}

// Läuft im WLAN-Task: Bestätigung (ACK) des Empfängers auswerten
static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    if (s_mode == LINK_RECEIVER && s_task != NULL) {
        xTaskNotify(s_task, status == ESP_NOW_SEND_SUCCESS ? NOTIFY_ACK : NOTIFY_FAIL,
                    eSetValueWithOverwrite);
    } else if (s_mode == LINK_GATEWAY && status != ESP_NOW_SEND_SUCCESS &&
               tx_info != NULL && tx_info->des_addr != NULL) {
        ESP_LOGW(TAG, "Empfänger " MACSTR " hat nicht bestätigt", MAC2STR(tx_info->des_addr));
    }
}

static esp_err_t add_peer(const uint8_t mac[ESPNOW_MAC_LEN])
{
    esp_now_peer_info_t peer = {
        .channel = 0,  // 0 = jeweils aktueller Kanal der Schnittstelle
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, mac, ESPNOW_MAC_LEN);
    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Peer " MACSTR " anlegen fehlgeschlagen: %s", MAC2STR(mac),
                 esp_err_to_name(err));
    }
    return err;
}

// Gemeinsamer Start: ESP-NOW an, Callbacks, Warteschlange
static esp_err_t start_common(void)
{
    esp_err_t err = esp_now_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_now_init fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    err = esp_now_register_recv_cb(recv_cb);
    if (err == ESP_OK) {
        err = esp_now_register_send_cb(send_cb);
    }
    if (err == ESP_OK) {
        s_queue = xQueueCreate(LINK_QUEUE_LEN, sizeof(link_event_t));
        if (s_queue == NULL) {
            err = ESP_ERR_NO_MEM;
        }
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP-NOW einrichten fehlgeschlagen: %s", esp_err_to_name(err));
        esp_now_deinit();
    }
    return err;
}

// Aufräumen nach einem Fehler beim Start
static void fail_cleanup(void)
{
    if (s_queue != NULL) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }
    s_mode = LINK_OFF;
    esp_now_deinit();
}

static esp_err_t send_palette_to(const uint8_t mac[ESPNOW_MAC_LEN], const led_palette_t *palette)
{
    uint8_t buf[ESPNOW_PROTO_MAX_LEN];
    int len = espnow_proto_encode_palette(palette, buf, sizeof(buf));
    if (len < 0) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = esp_now_send(mac, buf, (size_t)len);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Senden an " MACSTR " fehlgeschlagen: %s", MAC2STR(mac), esp_err_to_name(err));
    }
    return err;
}

// Beantwortet HELLOs: Palette senden, wenn der Empfänger einen anderen Stand hat
static void gateway_task(void *arg)
{
    link_event_t ev;
    while (true) {
        if (xQueueReceive(s_queue, &ev, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        led_palette_t palette;
        s_get_palette(&palette);
        if (led_palette_hash(&palette) != ev.msg.hash) {
            ESP_LOGI(TAG, "Empfänger " MACSTR " bekommt die aktuelle Palette", MAC2STR(ev.src));
            send_palette_to(ev.src, &palette);
        }
    }
}

esp_err_t espnow_link_gateway_start(const espnow_config_t *cfg,
                                    espnow_link_get_palette_t get_palette)
{
    if (cfg == NULL || get_palette == NULL || cfg->peer_count > ESPNOW_PEERS_MAX) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mode != LINK_OFF) {
        return ESP_ERR_INVALID_STATE;
    }
    // ESP-NOW läuft über die STA-Schnittstelle (deren MAC steht in den Einstellungen)
    wifi_mode_t mode = WIFI_MODE_NULL;
    esp_err_t err = esp_wifi_get_mode(&mode);
    if (err == ESP_OK && mode == WIFI_MODE_AP) {
        err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WLAN-Modus für ESP-NOW nicht einstellbar: %s", esp_err_to_name(err));
        return err;
    }

    s_cfg = *cfg;
    s_get_palette = get_palette;
    s_mode = LINK_GATEWAY;
    err = start_common();
    if (err != ESP_OK) {
        s_mode = LINK_OFF;
        return err;
    }
    for (size_t i = 0; i < cfg->peer_count && err == ESP_OK; i++) {
        err = add_peer(cfg->peers[i]);
    }
    if (err == ESP_OK &&
        xTaskCreate(gateway_task, "espnow_gw", LINK_TASK_STACK, NULL, LINK_TASK_PRIO,
                    &s_task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
    }
    if (err != ESP_OK) {
        fail_cleanup();
        return err;
    }
    ESP_LOGI(TAG, "Gateway für %u Empfänger bereit", (unsigned)cfg->peer_count);
    return ESP_OK;
}

esp_err_t espnow_link_gateway_send(const led_palette_t *palette)
{
    if (palette == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mode != LINK_GATEWAY) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t result = ESP_OK;
    for (size_t i = 0; i < s_cfg.peer_count; i++) {
        esp_err_t err = send_palette_to(s_cfg.peers[i], palette);
        if (err == ESP_ERR_INVALID_ARG) {
            return err;
        }
        if (err != ESP_OK) {
            result = err;
        }
    }
    return result;
}

static void set_channel(uint8_t channel)
{
    esp_err_t err = esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Kanal %u einstellen fehlgeschlagen: %s", channel, esp_err_to_name(err));
    }
}

// Sendet ein HELLO und wartet auf die Bestätigung; true = Gateway hat es empfangen
static bool send_hello(void)
{
    led_palette_t palette;
    s_get_palette(&palette);
    uint8_t buf[ESPNOW_PROTO_HEADER_LEN];
    int len = espnow_proto_encode_hello(led_palette_hash(&palette), buf, sizeof(buf));
    // Alte Bestätigungen (nach Zeitüberschreitung) verwerfen
    xTaskNotifyStateClear(NULL);
    ulTaskNotifyValueClear(NULL, UINT32_MAX);
    if (len < 0 || esp_now_send(s_cfg.gateway, buf, (size_t)len) != ESP_OK) {
        return false;
    }
    uint32_t value = 0;
    if (xTaskNotifyWait(0, UINT32_MAX, &value, pdMS_TO_TICKS(s_rcfg.ack_timeout_ms)) != pdTRUE) {
        return false;
    }
    return value == NOTIFY_ACK;
}

// Empfangene Paletten bis zum Ablauf von wait_ms abarbeiten
static void handle_palettes(uint32_t wait_ms)
{
    const TickType_t start = xTaskGetTickCount();
    const TickType_t wait = pdMS_TO_TICKS(wait_ms);
    link_event_t ev;
    while (true) {
        TickType_t elapsed = xTaskGetTickCount() - start;
        if (elapsed >= wait || xQueueReceive(s_queue, &ev, wait - elapsed) != pdTRUE) {
            return;
        }
        led_palette_t current;
        s_get_palette(&current);
        if (led_palette_hash(&current) != ev.msg.hash && s_on_palette != NULL) {
            ESP_LOGI(TAG, "Neue Palette vom Gateway (%u Farben)", (unsigned)ev.msg.palette.count);
            s_on_palette(&ev.msg.palette);
        }
    }
}

// Kanalsuche, Lebenszeichen und Empfang
static void receiver_task(void *arg)
{
    set_channel(s_scan.channel);
    while (true) {
        espnow_scan_state_t before = s_scan.state;
        bool acked = send_hello();
        if (espnow_scan_result(&s_scan, acked)) {
            set_channel(s_scan.channel);
        }
        if (before != s_scan.state) {
            if (s_scan.state == ESPNOW_SCAN_CONNECTED) {
                ESP_LOGI(TAG, "Gateway auf Kanal %u gefunden", s_scan.channel);
            } else {
                ESP_LOGW(TAG, "Gateway antwortet nicht mehr, suche neu");
            }
        }
        atomic_store(&s_connected, s_scan.state == ESPNOW_SCAN_CONNECTED);
        handle_palettes(espnow_scan_wait_ms(&s_scan));
    }
}

esp_err_t espnow_link_receiver_start(const espnow_config_t *cfg,
                                     const espnow_link_receiver_cfg_t *rcfg,
                                     espnow_link_get_palette_t get_palette,
                                     espnow_link_on_palette_t on_palette)
{
    if (cfg == NULL || rcfg == NULL || get_palette == NULL ||
        !espnow_mac_is_set(cfg->gateway)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (s_mode != LINK_OFF) {
        return ESP_ERR_INVALID_STATE;
    }
    // AP aus, STA ohne Verbindung: Kanal ist dann frei wählbar
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "STA-Modus fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }
    // Im Modem-Sleep gehen ESP-NOW-Pakete verloren. Nur beim Empfänger
    // abschalten: mit Bluetooth (Gateway) verlangt ESP-IDF den Modem-Sleep.
    err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Stromsparen nicht abschaltbar: %s", esp_err_to_name(err));
    }
    if (rcfg->country != NULL) {
        err = esp_wifi_set_country_code(rcfg->country, false);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Ländercode %s nicht gesetzt: %s", rcfg->country, esp_err_to_name(err));
        }
    }

    s_cfg = *cfg;
    s_rcfg = *rcfg;
    s_scan = rcfg->scan;
    espnow_scan_init(&s_scan);
    s_get_palette = get_palette;
    s_on_palette = on_palette;
    atomic_store(&s_connected, false);
    s_mode = LINK_RECEIVER;
    err = start_common();
    if (err != ESP_OK) {
        s_mode = LINK_OFF;
        return err;
    }
    err = add_peer(cfg->gateway);
    if (err == ESP_OK &&
        xTaskCreate(receiver_task, "espnow_rx", LINK_TASK_STACK, NULL, LINK_TASK_PRIO,
                    &s_task) != pdPASS) {
        err = ESP_ERR_NO_MEM;
    }
    if (err != ESP_OK) {
        fail_cleanup();
        return err;
    }
    ESP_LOGI(TAG, "Empfänger aktiv, suche Gateway " MACSTR, MAC2STR(cfg->gateway));
    return ESP_OK;
}

bool espnow_link_receiver_connected(void)
{
    return s_mode == LINK_RECEIVER && atomic_load(&s_connected);
}

void espnow_link_stop(void)
{
    if (s_mode == LINK_OFF) {
        return;
    }
    if (s_task != NULL) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    esp_now_unregister_recv_cb();
    esp_now_unregister_send_cb();
    if (s_queue != NULL) {
        vQueueDelete(s_queue);
        s_queue = NULL;
    }
    esp_now_deinit();
    s_mode = LINK_OFF;
    atomic_store(&s_connected, false);
}
