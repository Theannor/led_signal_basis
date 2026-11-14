// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Theannor
#include <string.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "bootloader_random.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_random.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "ble_api.h"
#include "ble_gatt.h"
#include "ble_name_store.h"
#include "config_store.h"
#include "espnow_config.h"
#include "espnow_link.h"
#include "espnow_store.h"
#include "led_color.h"
#include "led_config.h"
#include "led_output.h"
#include "led_palette.h"
#include "led_password.h"
#include "led_store.h"
#include "led_twinkle.h"
#include "net_config.h"
#include "net_name.h"
#include "palette_store.h"
#include "password_store.h"
#include "web_server.h"
#include "wifi_ap.h"
#include "wifi_sta.h"

// Hardware-Konfiguration (GPIO und Anzahl sind auf der Webseite einstellbar)
#define G(n) LED_CONFIG_GPIO_BIT(n)
#if CONFIG_IDF_TARGET_ESP32
// Datenleitung des WS2812B-Strips, wenn nichts gespeichert ist
#define LED_GPIO_DEFAULT    21
// Wählbare GPIOs: ohne Flash (6..11), Strapping (0, 2, 5, 12, 15),
// UART0 (1, 3) und reine Eingänge (34..39)
#define LED_GPIO_ALLOWED    (G(4) | G(13) | G(14) | G(16) | G(17) | G(18) | G(19) | G(21) | \
                             G(22) | G(23) | G(25) | G(26) | G(27) | G(32) | G(33))
#else
#define LED_GPIO_DEFAULT    10
// ESP32-C3: ohne Strapping (2, 8, 9), Flash (12..17), USB (18, 19) und UART0 (20, 21)
#define LED_GPIO_ALLOWED    (G(0) | G(1) | G(3) | G(4) | G(5) | G(6) | G(7) | G(10))
#endif
#define LED_COUNT_DEFAULT   16  // Anzahl LEDs, wenn nichts gespeichert ist
#define LED_MAX_BRIGHTNESS  25  // Obergrenze wegen USB-Versorgung (von 255)

// Eigener Access Point (wenn kein Client-WLAN erreichbar ist)
#define AP_SSID_PREFIX      "led_signal_basis"  // + "-" + letzte MAC-Stellen
#define AP_CHANNEL          1
#define AP_MAX_CLIENTS      4
#define AP_IP               "192.168.4.1"  // Standard-Adresse des ESP-IDF-AP

// WLAN-Client
#define DEFAULT_HOSTNAME        "led-signal-basis"
#define STA_CONNECT_TIMEOUT_MS  15000   // so lange beim Start auf das WLAN warten
#define STA_FALLBACK_MS         120000  // so lange ohne Verbindung -> Neustart

// Webseite
#define WEB_PORT            80
#define RESTART_DELAY_MS    2000  // Zeit, damit die Antwortseite noch ankommt

// Bluetooth
#define BLE_DEFAULT_NAME    "LED Signal"  // solange kein Name gespeichert ist

// ESP-NOW
#define ESPNOW_WINDOW_MS         60000  // Empfänger: AP so lange nach dem Start bzw. letzter Abmeldung offen
#define ESPNOW_WINDOW_CHECK_MS   500    // Takt der Prüfung, ob das Fenster abgelaufen ist
#define ESPNOW_MAX_CHANNEL       13     // höchster WLAN-Kanal (Europa)
#define ESPNOW_COUNTRY           "DE"   // Ländercode, damit die Kanäle 12 und 13 erlaubt sind
#define ESPNOW_TRIES_PER_CHANNEL 5      // HELLOs je Kanal (Gateway im Modem-Sleep verpasst einzelne)
#define ESPNOW_RETRY_MS          60     // Abstand der HELLOs bei der Suche
#define ESPNOW_ACK_TIMEOUT_MS    50     // so lange auf die Bestätigung warten
#define ESPNOW_KEEPALIVE_MS      30000  // Lebenszeichen, wenn das Gateway gefunden ist
#define ESPNOW_MAX_MISSES        3      // so viele unbestätigte Lebenszeichen -> neu suchen
#define ESPNOW_TASK_STACK        3072

// Anzeige
#define DISPLAY_PERIOD_MS   50    // Takt der Anzeige-Schleife
#define PASSWORD_STEP_MS    1000  // so lange ist jeweils eine LED weiß
#define GREEN_FLASH_MS      2000  // grünes Signal nach erfolgreicher Verbindung
#define TWINKLE_MIN_FADE_MS 1000  // kürzeste Ein-/Ausblendzeit einer LED
#define TWINKLE_MAX_FADE_MS 4000  // längste Ein-/Ausblendzeit einer LED
#define SEARCH_PULSE_MS     2000  // Empfänger sucht das Gateway: blaues Pulsieren
#define OFF_CHECK_MS        500   // Palette "aus": so selten Zustände prüfen (Palettenwechsel weckt sofort)
#define DISPLAY_TASK_STACK  3072
#define DISPLAY_TASK_PRIO   5
#define RESTART_TASK_STACK  2048

// Jeder Passwort-Buchstabe braucht eine eigene LED
_Static_assert(LED_CONFIG_COUNT_MIN >= LED_PASSWORD_LEN, "zu wenige LEDs für das Passwort");
_Static_assert(LED_COUNT_DEFAULT >= LED_CONFIG_COUNT_MIN && LED_COUNT_DEFAULT <= LED_CONFIG_COUNT_MAX,
               "LED_COUNT_DEFAULT außerhalb des erlaubten Bereichs");
_Static_assert((LED_GPIO_ALLOWED & G(LED_GPIO_DEFAULT)) != 0, "LED_GPIO_DEFAULT nicht wählbar");

static const char *TAG = "main";

static char s_ap_password[LED_PASSWORD_LEN + 1];
static led_config_t s_led;  // GPIO und Anzahl der LEDs, beim Start geladen
static bool s_client_mode = false;

// ESP-NOW: Rolle aus den Einstellungen, Zustand zur Laufzeit
static espnow_config_t s_espnow;
static bool s_receiver_mode = false;           // Rolle Empfänger
static volatile bool s_receiver_active = false;  // Fenster vorbei, ESP-NOW-Empfang läuft
static bool s_gateway_active = false;          // leitet Paletten an Empfänger weiter

// Farbpalette fürs Funkeln: vom Webserver gesetzt, von der Anzeige gelesen
static SemaphoreHandle_t s_palette_lock;
static led_palette_t s_palette;
static uint32_t s_palette_version;  // wird bei jeder Änderung erhöht
static TaskHandle_t s_display_task;  // wird bei neuer Palette geweckt

static uint32_t now_ms(void)
{
    return (uint32_t)pdTICKS_TO_MS(xTaskGetTickCount());
}

// Die ersten count LEDs in einer Farbe
static void fill(led_color_t *colors, size_t count, led_color_t color)
{
    for (size_t i = 0; i < count; i++) {
        colors[i] = led_color_scale(color, LED_MAX_BRIGHTNESS);
    }
}

// Vom Webserver oder per Bluetooth nach dem Speichern einer neuen Palette aufgerufen
static void on_palette_changed(const led_palette_t *palette)
{
    xSemaphoreTake(s_palette_lock, portMAX_DELAY);
    s_palette = *palette;
    s_palette_version++;
    xSemaphoreGive(s_palette_lock);
    // Anzeige wecken, falls sie im Zustand "aus" lange schläft
    if (s_display_task != NULL) {
        xTaskNotifyGive(s_display_task);
    }
    // Webseite auf denselben Stand bringen (bei Änderung per Bluetooth);
    // beim aktiven Empfänger läuft keine Webseite mehr
    if (!s_receiver_active) {
        esp_err_t err = web_server_set_palette(palette);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Palette für die Webseite nicht übernommen: %s", esp_err_to_name(err));
        }
    }
    // An die ESP-NOW-Empfänger weiterleiten
    if (s_gateway_active) {
        esp_err_t err = espnow_link_gateway_send(palette);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Weiterleiten per ESP-NOW unvollständig: %s", esp_err_to_name(err));
        }
    }
    ESP_LOGI(TAG, "Neue Palette mit %u Farben", (unsigned)palette->count);
}

// Per ESP-NOW empfangene Palette speichern (für den nächsten Start) und anzeigen
static void on_espnow_palette(const led_palette_t *palette)
{
    esp_err_t err = palette_store_save(palette);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Empfangene Palette nicht gespeichert: %s", esp_err_to_name(err));
    }
    on_palette_changed(palette);
}

// Aktuelle Palette für Lesezugriffe per Bluetooth und ESP-NOW
static void get_palette(led_palette_t *out)
{
    xSemaphoreTake(s_palette_lock, portMAX_DELAY);
    *out = s_palette;
    xSemaphoreGive(s_palette_lock);
}

// Kopiert die Palette nach local, wenn sie sich seit *version geändert hat
static void sync_palette(led_palette_t *local, uint32_t *version)
{
    xSemaphoreTake(s_palette_lock, portMAX_DELAY);
    if (*version != s_palette_version) {
        *local = s_palette;
        *version = s_palette_version;
    }
    xSemaphoreGive(s_palette_lock);
}

// AP-Modus: ohne verbundenes Gerät Passwort zeigen (eine LED nach der anderen
// kurz weiß), sonst Funkeln.
// ESP-NOW-Empfänger: im Einstellungsfenster wie AP-Modus, danach blau
// pulsierend, solange das Gateway gesucht wird, sonst Funkeln.
// Client-Modus: kurz grün, dann Funkeln; nach STA_FALLBACK_MS ohne
// Verbindung Neustart (beim Start wird dann neu zwischen Client und AP entschieden).
// Sonderfall Palette nur Schwarz ("aus"): statt Funkeln einmal alle LEDs
// ausschalten, danach nichts mehr berechnen oder senden und nur alle
// OFF_CHECK_MS die Zustände prüfen; eine neue Palette weckt die Task sofort.
static void display_task(void *arg)
{
    const size_t count = s_led.count;
    led_color_t colors[LED_CONFIG_COUNT_MAX];
    // Funkeln: jede LED blendet eine Farbe aus der Palette (leer = beliebiger
    // Farbton) ein und wieder aus
    static led_palette_t palette;
    uint32_t palette_version = 0;
    sync_palette(&palette, &palette_version);
    led_twinkle_cfg_t twinkle_cfg = {
        .rand_fn = esp_random,
        .min_fade_ms = TWINKLE_MIN_FADE_MS,
        .max_fade_ms = TWINKLE_MAX_FADE_MS,
        .palette = palette.colors,
        .palette_count = palette.count,
    };
    led_twinkle_t twinkle[LED_CONFIG_COUNT_MAX];
    for (size_t i = 0; i < count; i++) {
        led_twinkle_start(&twinkle[i], &twinkle_cfg);
    }
    size_t highlight = 0;
    uint32_t highlight_ms = 0;
    bool showing_password = false;
    bool showing_off = false;  // Strip ist bereits ganz dunkel
    bool palette_was_off = false;  // Funkel-Zyklen laufen noch mit Schwarz
    const uint32_t started_ms = now_ms();

    while (true) {
        bool receiver_active = s_receiver_active;
        bool show_password = !s_client_mode && !receiver_active && wifi_ap_client_count() == 0;
        bool show_search = receiver_active && !espnow_link_receiver_connected();
        bool show_green = s_client_mode && (now_ms() - started_ms) < GREEN_FLASH_MS;

        if (s_client_mode && net_should_fall_back(now_ms(), wifi_sta_lost_at_ms(),
                                                  wifi_sta_is_connected(), STA_FALLBACK_MS)) {
            ESP_LOGW(TAG, "WLAN seit %d s weg, starte neu", STA_FALLBACK_MS / 1000);
            esp_restart();
        }

        uint32_t wait_ms = DISPLAY_PERIOD_MS;
        bool send = true;
        bool off = false;  // Palette "aus" wird gezeigt
        if (show_password) {
            // Beim Wechsel zur Passwort-Anzeige wieder bei LED 0 beginnen
            if (!showing_password) {
                showing_password = true;
                highlight = 0;
                highlight_ms = 0;
            }
            // Passwort auf den ersten LED_PASSWORD_LEN LEDs, die übrigen aus
            memset(colors, 0, sizeof(colors));
            led_password_frame(s_ap_password, highlight, LED_MAX_BRIGHTNESS, colors);
            highlight_ms += DISPLAY_PERIOD_MS;
            if (highlight_ms >= PASSWORD_STEP_MS) {
                highlight_ms = 0;
                highlight = (highlight + 1) % LED_PASSWORD_LEN;
            }
        } else if (show_green) {
            fill(colors, count, (led_color_t){.r = 0, .g = 255, .b = 0});
        } else if (show_search) {
            uint8_t level = led_pulse_level(now_ms(), SEARCH_PULSE_MS);
            fill(colors, count, led_color_scale((led_color_t){.r = 0, .g = 0, .b = 255}, level));
        } else {
            showing_password = false;
            // Neue Palette gilt ab dem nächsten Zyklus der jeweiligen LED
            sync_palette(&palette, &palette_version);
            twinkle_cfg.palette_count = palette.count;
            if (led_palette_is_off(&palette)) {
                off = true;
                palette_was_off = true;
                // Nur beim Wechsel nach "aus" einmal Schwarz senden
                send = !showing_off;
                if (send) {
                    memset(colors, 0, sizeof(colors));
                    ESP_LOGI(TAG, "LEDs aus");
                }
                wait_ms = OFF_CHECK_MS;
            } else {
                // Nach "aus" sofort mit neuen Farben beginnen statt erst
                // die schwarzen Zyklen zu Ende laufen zu lassen
                if (palette_was_off) {
                    palette_was_off = false;
                    for (size_t i = 0; i < count; i++) {
                        led_twinkle_start(&twinkle[i], &twinkle_cfg);
                    }
                }
                led_twinkle_update(twinkle, count, DISPLAY_PERIOD_MS, &twinkle_cfg,
                                   LED_MAX_BRIGHTNESS, colors);
            }
        }

        if (send) {
            esp_err_t err = led_output_show(colors, count);
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "Anzeige fehlgeschlagen: %s", esp_err_to_name(err));
            }
            // Bei Fehler im nächsten Durchlauf erneut versuchen
            showing_off = off && err == ESP_OK;
        }
        // Wartet blockierend (kein Busy-Loop); eine neue Palette weckt vorzeitig
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));
    }
}

// Startet nach kurzer Pause neu (damit die Antwortseite noch gesendet wird)
static void restart_task(void *arg)
{
    vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
    ESP_LOGI(TAG, "Neustart nach geänderter Konfiguration");
    esp_restart();
}

static void schedule_restart(void)
{
    if (xTaskCreate(restart_task, "restart", RESTART_TASK_STACK, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Neustart-Task konnte nicht gestartet werden, starte sofort neu");
        esp_restart();
    }
}

// Empfänger: wartet, bis der AP ESPNOW_WINDOW_MS lang ohne verbundenes Gerät
// war, schaltet dann Webseite, mDNS und AP ab und startet den ESP-NOW-Empfang
static void receiver_window_task(void *arg)
{
    ESP_LOGI(TAG, "Einstellungen %d s lang über den Access Point erreichbar",
             ESPNOW_WINDOW_MS / 1000);
    while (!net_should_fall_back(now_ms(), wifi_ap_last_left_ms(),
                                 wifi_ap_client_count() > 0, ESPNOW_WINDOW_MS)) {
        vTaskDelay(pdMS_TO_TICKS(ESPNOW_WINDOW_CHECK_MS));
    }
    ESP_LOGI(TAG, "Einstellungsfenster vorbei, wechsle in den ESP-NOW-Empfang");
    s_receiver_active = true;
    esp_err_t err = web_server_stop();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Webseite stoppen fehlgeschlagen: %s", esp_err_to_name(err));
    }
    net_name_stop();

    espnow_link_receiver_cfg_t rcfg = {
        .scan = {
            .max_channel = ESPNOW_MAX_CHANNEL,
            .tries_per_channel = ESPNOW_TRIES_PER_CHANNEL,
            .max_misses = ESPNOW_MAX_MISSES,
            .retry_ms = ESPNOW_RETRY_MS,
            .keepalive_ms = ESPNOW_KEEPALIVE_MS,
        },
        .ack_timeout_ms = ESPNOW_ACK_TIMEOUT_MS,
        .country = ESPNOW_COUNTRY,
    };
    err = espnow_link_receiver_start(&s_espnow, &rcfg, get_palette, on_espnow_palette);
    if (err != ESP_OK) {
        // Ohne Empfang hilft nur ein Neustart (öffnet auch das Fenster wieder)
        ESP_LOGE(TAG, "ESP-NOW-Empfang nicht gestartet (%s), starte neu", esp_err_to_name(err));
        vTaskDelay(pdMS_TO_TICKS(RESTART_DELAY_MS));
        esp_restart();
    }
    vTaskDelete(NULL);
}

static void ap_ip(char *buf, size_t len)
{
    strlcpy(buf, AP_IP, len);
}

// NVS starten; bei voller oder veralteter Partition löschen und neu anlegen
static void init_nvs(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS wird neu angelegt (%s)", esp_err_to_name(err));
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void app_main(void)
{
    init_nvs();

    const led_config_t led_defaults = {.gpio = LED_GPIO_DEFAULT, .count = LED_COUNT_DEFAULT};
    ESP_ERROR_CHECK(led_store_load(&s_led, &led_defaults, LED_GPIO_ALLOWED));
    ESP_LOGI(TAG, "LED-Strip: GPIO %u, %u LEDs, max. Helligkeit %d/255",
             (unsigned)s_led.gpio, (unsigned)s_led.count, LED_MAX_BRIGHTNESS);

    // Echte Zufallszahlen gibt es erst mit aktivem Funk; bis dahin die
    // Rauschquelle des Bootloaders nutzen. Muss vor dem WLAN-Start wieder aus.
    bootloader_random_enable();
    esp_err_t err = password_store_get(s_ap_password, esp_random);
    bootloader_random_disable();
    ESP_ERROR_CHECK(err);

    net_config_t cfg;
    ESP_ERROR_CHECK(config_store_load(&cfg, DEFAULT_HOSTNAME));
    ESP_ERROR_CHECK(espnow_store_load(&s_espnow));
    s_receiver_mode = s_espnow.role == ESPNOW_ROLE_RECEIVER;

    // ESP-NOW nutzt die STA-Schnittstelle; ihre MAC tragen andere Geräte ein
    uint8_t own_mac[ESPNOW_MAC_LEN];
    ESP_ERROR_CHECK(esp_read_mac(own_mac, ESP_MAC_WIFI_STA));
    char own_mac_text[ESPNOW_MAC_STR_LEN];
    espnow_mac_format(own_mac, own_mac_text);
    ESP_LOGI(TAG, "ESP-NOW: MAC %s, Rolle %s", own_mac_text,
             s_receiver_mode ? "Empfänger" : "Gateway");
    char ap_ssid[NET_CONFIG_SSID_MAX + 1];
    if (!net_ap_ssid(AP_SSID_PREFIX, own_mac, ap_ssid, sizeof(ap_ssid))) {
        strlcpy(ap_ssid, AP_SSID_PREFIX, sizeof(ap_ssid));
    }
    char ble_name[NET_CONFIG_BLE_NAME_MAX + 1];
    ESP_ERROR_CHECK(ble_name_store_load(ble_name, sizeof(ble_name), BLE_DEFAULT_NAME));

    s_palette_lock = xSemaphoreCreateMutex();
    if (s_palette_lock == NULL) {
        ESP_ERROR_CHECK(ESP_ERR_NO_MEM);
    }
    ESP_ERROR_CHECK(palette_store_load(&s_palette));
    s_palette_version = 1;

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // LEDs behalten nach einem Neustart ihre Farbe -> während des Verbindens aus
    ESP_ERROR_CHECK(led_output_init(s_led.gpio, s_led.count));
    led_color_t off[LED_CONFIG_COUNT_MAX] = {0};
    led_output_show(off, s_led.count);

    // Erst das gespeicherte WLAN versuchen, sonst eigener Access Point.
    // Empfänger nutzen das WLAN nicht, sondern öffnen nur kurz den AP.
    if (!s_receiver_mode && cfg.ssid[0] != '\0' &&
        wifi_sta_connect(&cfg, STA_CONNECT_TIMEOUT_MS) == ESP_OK) {
        s_client_mode = true;
        ESP_LOGI(TAG, "Modus: WLAN-Client in \"%s\"", cfg.ssid);
    } else {
        ESP_ERROR_CHECK(wifi_ap_start(ap_ssid, s_ap_password, AP_CHANNEL, AP_MAX_CLIENTS));
        ESP_LOGI(TAG, "Modus: Access Point \"%s\"", ap_ssid);
    }

    if (net_name_start(cfg.hostname, WEB_PORT) != ESP_OK) {
        ESP_LOGW(TAG, "Kein mDNS; Webseite nur über die IP-Adresse erreichbar");
    }

    web_server_ctx_t web = {
        .client_mode = s_client_mode,
        .port = WEB_PORT,
        .config = cfg,
        .get_ip = s_client_mode ? wifi_sta_ip : ap_ip,
        .schedule_restart = schedule_restart,
        .palette = s_palette,
        .on_palette_changed = on_palette_changed,
        .espnow = s_espnow,
        .receiver_mode = s_receiver_mode,
        .espnow_window_s = ESPNOW_WINDOW_MS / 1000,
        .led = s_led,
        .led_gpio_mask = LED_GPIO_ALLOWED,
    };
    memcpy(web.own_mac, own_mac, sizeof(web.own_mac));
    // Empfänger haben kein Bluetooth -> Bereich auf der Seite weglassen
    if (!s_receiver_mode) {
        strlcpy(web.ble_name, ble_name, sizeof(web.ble_name));
    }
    ESP_ERROR_CHECK(web_server_start(&web));

    if (s_receiver_mode) {
        // Farben kommen nur per ESP-NOW, nach Ablauf des Einstellungsfensters
        if (xTaskCreate(receiver_window_task, "rx_window", ESPNOW_TASK_STACK,
                        NULL, DISPLAY_TASK_PRIO, NULL) != pdPASS) {
            ESP_LOGE(TAG, "Empfänger-Task konnte nicht gestartet werden");
        }
    } else {
        // Bluetooth ist optional: ohne läuft der Rest normal weiter
        ble_api_ctx_t ble = {
            .get_palette = get_palette,
            .on_palette_changed = on_palette_changed,
        };
        ESP_ERROR_CHECK(ble_api_init(&ble));
        if (ble_gatt_start(ble_name) != ESP_OK) {
            ESP_LOGW(TAG, "Bluetooth nicht verfügbar; Farben nur über die Webseite");
        }
        // Weiterleiten an ESP-NOW-Empfänger ist optional
        if (s_espnow.peer_count > 0) {
            if (espnow_link_gateway_start(&s_espnow, get_palette) == ESP_OK) {
                s_gateway_active = true;
            } else {
                ESP_LOGW(TAG, "ESP-NOW nicht verfügbar; keine Weiterleitung");
            }
        }
    }

    BaseType_t ok = xTaskCreate(display_task, "display", DISPLAY_TASK_STACK,
                                NULL, DISPLAY_TASK_PRIO, &s_display_task);
    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Anzeige-Task konnte nicht gestartet werden");
    }
}
