// Globale Symbole, die sonst die WLAN-Bibliothek liefert
// (gebraucht von WIFI_INIT_CONFIG_DEFAULT() und dem Event-Basisnamen)
#include "esp_wifi.h"
#include "esp_private/wifi_os_adapter.h"

const wpa_crypto_funcs_t g_wifi_default_wpa_crypto_funcs = { };
wifi_osi_funcs_t g_wifi_osi_funcs = { };

ESP_EVENT_DEFINE_BASE(WIFI_EVENT);
