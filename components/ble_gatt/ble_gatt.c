#include <string.h>
#include "ble_gatt.h"
#include "ble_api.h"
#include "esp_log.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

// Dienst "LED Signal Palette": a4ace06f-4b89-4738-86cf-f71f6f6d39e7
// (BLE_UUID128_INIT erwartet die Bytes in umgekehrter Reihenfolge)
#define PALETTE_SVC_UUID  BLE_UUID128_INIT(0xe7, 0x39, 0x6d, 0x6f, 0x1f, 0xf7, 0xcf, 0x86, \
                                           0x38, 0x47, 0x89, 0x4b, 0x6f, 0xe0, 0xac, 0xa4)
// Characteristic "Palette" (Lesen/Schreiben): e5e16445-2813-4387-904d-6624f8aa0f03
#define PALETTE_CHR_UUID  BLE_UUID128_INIT(0x03, 0x0f, 0xaa, 0xf8, 0x24, 0x66, 0x4d, 0x90, \
                                           0x87, 0x43, 0x13, 0x28, 0x45, 0x64, 0xe1, 0xe5)

#define ADV_INTERVAL_MIN_MS  200
#define ADV_INTERVAL_MAX_MS  300

static const char *TAG = "ble_gatt";

static const ble_uuid128_t s_svc_uuid = PALETTE_SVC_UUID;
static const ble_uuid128_t s_chr_uuid = PALETTE_CHR_UUID;
static uint16_t s_chr_val_handle;
static uint8_t s_own_addr_type;

static int palette_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg);
static void start_advertising(void);

static const struct ble_gatt_svc_def s_services[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &s_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]){
            {
                .uuid = &s_chr_uuid.u,
                .access_cb = palette_access,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .val_handle = &s_chr_val_handle,
            },
            {0},  // Ende der Characteristics
        },
    },
    {0},  // Ende der Dienste
};

// Lese- und Schreibzugriffe auf die Palette an ble_api weiterreichen
static int palette_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint8_t buf[BLE_API_PALETTE_MAX_LEN];

    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) {
        int len = ble_api_palette_read(buf, sizeof(buf));
        if (len < 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        return os_mbuf_append(ctxt->om, buf, (uint16_t)len) == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        // Lange Schreibvorgänge kommen als Kette von mbufs -> erst Länge prüfen
        uint16_t total = OS_MBUF_PKTLEN(ctxt->om);
        if (total > sizeof(buf) || total % 3 != 0) {
            ESP_LOGW(TAG, "Palette mit ungültiger Länge %u abgelehnt", (unsigned)total);
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        uint16_t len = 0;
        if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &len) != 0) {
            return BLE_ATT_ERR_UNLIKELY;
        }
        esp_err_t err = ble_api_palette_write(buf, len);
        if (err == ESP_ERR_INVALID_SIZE) {
            return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Palette setzen fehlgeschlagen: %s", esp_err_to_name(err));
            return BLE_ATT_ERR_UNLIKELY;
        }
        return 0;
    }

    return BLE_ATT_ERR_UNLIKELY;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        ESP_LOGI(TAG, "Verbindung %s (Status %d)",
                 event->connect.status == 0 ? "aufgebaut" : "fehlgeschlagen",
                 event->connect.status);
        if (event->connect.status != 0) {
            start_advertising();
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Verbindung getrennt (Grund 0x%x)", event->disconnect.reason);
        start_advertising();
        break;
    case BLE_GAP_EVENT_ADV_COMPLETE:
        start_advertising();
        break;
    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU %u", event->mtu.value);
        break;
    default:
        break;
    }
    return 0;
}

// Name im Advertising-Paket, Dienst-UUID in der Scan-Antwort (je max. 31 Bytes)
static void start_advertising(void)
{
    struct ble_hs_adv_fields fields = {0};
    const char *name = ble_svc_gap_device_name();
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)name;
    fields.name_len = (uint8_t)strlen(name);
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "Advertising-Daten setzen fehlgeschlagen: %d", rc);
        return;
    }

    struct ble_hs_adv_fields rsp = {0};
    rsp.uuids128 = &s_svc_uuid;
    rsp.num_uuids128 = 1;
    rsp.uuids128_is_complete = 1;
    rc = ble_gap_adv_rsp_set_fields(&rsp);
    if (rc != 0) {
        ESP_LOGE(TAG, "Scan-Antwort setzen fehlgeschlagen: %d", rc);
        return;
    }

    struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
        .itvl_min = BLE_GAP_ADV_ITVL_MS(ADV_INTERVAL_MIN_MS),
        .itvl_max = BLE_GAP_ADV_ITVL_MS(ADV_INTERVAL_MAX_MS),
    };
    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) {
        ESP_LOGE(TAG, "Advertising starten fehlgeschlagen: %d", rc);
    }
}

// Host und Controller sind synchron -> Adresse bestimmen und werben
static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "Keine Bluetooth-Adresse verfügbar: %d", rc);
        return;
    }
    ESP_LOGI(TAG, "Bluetooth wirbt als \"%s\"", ble_svc_gap_device_name());
    start_advertising();
}

static void on_reset(int reason)
{
    ESP_LOGW(TAG, "NimBLE-Stack zurückgesetzt (Grund %d)", reason);
}

static void host_task(void *param)
{
    // Läuft, bis nimble_port_stop() aufgerufen wird
    nimble_port_run();
    nimble_port_freertos_deinit();
}

esp_err_t ble_gatt_start(const char *device_name)
{
    if (device_name == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NimBLE starten fehlgeschlagen: %s", esp_err_to_name(err));
        return err;
    }

    ble_hs_cfg.sync_cb = on_sync;
    ble_hs_cfg.reset_cb = on_reset;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_svc_gap_device_name_set(device_name);
    if (rc == 0) {
        rc = ble_gatts_count_cfg(s_services);
    }
    if (rc == 0) {
        rc = ble_gatts_add_svcs(s_services);
    }
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT-Dienst anlegen fehlgeschlagen: %d", rc);
        nimble_port_deinit();
        return ESP_FAIL;
    }

    nimble_port_freertos_init(host_task);
    return ESP_OK;
}
