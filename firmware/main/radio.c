#include "radio.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"
#include "sdkconfig.h"

#if CONFIG_DOT_BLUETOOTH
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#endif

static const char* TAG = "radio";

static volatile radio_state_t wifi_state = RADIO_OFF;
static volatile radio_state_t ble_state = RADIO_OFF;

radio_state_t radio_wifi_state(void) {
    return wifi_state;
}

radio_state_t radio_ble_state(void) {
    return ble_state;
}

#if CONFIG_DOT_WIFI
static void on_wifi_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        wifi_state = RADIO_WORKING;
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_state = RADIO_WORKING;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        wifi_state = RADIO_CONNECTED;
    }
}

static void wifi_start(void) {
    if (CONFIG_DOT_WIFI_SSID[0] == '\0') {
        ESP_LOGI(TAG, "Wi-Fi off: no network set under Dot: Radios in menuconfig");
        return;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_netif_init());
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "event loop: %s", esp_err_to_name(err));
        return;
    }
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&init);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi init failed: %s", esp_err_to_name(err));
        return;
    }
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL));
    ESP_ERROR_CHECK_WITHOUT_ABORT(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_wifi_event, NULL));

    wifi_config_t cfg = {0};
    strlcpy((char*)cfg.sta.ssid, CONFIG_DOT_WIFI_SSID, sizeof(cfg.sta.ssid));
    strlcpy((char*)cfg.sta.password, CONFIG_DOT_WIFI_PASSWORD, sizeof(cfg.sta.password));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_set_config(WIFI_IF_STA, &cfg));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_wifi_start());
}
#endif

#if CONFIG_DOT_BLUETOOTH
static uint8_t own_addr_type;

static void advertise(void);

static int on_gap_event(struct ble_gap_event* event, void* arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0)
                ble_state = RADIO_CONNECTED;
            else
                advertise();
            break;
        case BLE_GAP_EVENT_DISCONNECT:
        case BLE_GAP_EVENT_ADV_COMPLETE:
            advertise();
            break;
        default:
            break;
    }
    return 0;
}

static void advertise(void) {
    const char* name = ble_svc_gap_device_name();
    struct ble_hs_adv_fields fields = {0};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t*)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    const struct ble_gap_adv_params params = {
        .conn_mode = BLE_GAP_CONN_MODE_UND,
        .disc_mode = BLE_GAP_DISC_MODE_GEN,
    };
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc == 0)
        rc = ble_gap_adv_start(own_addr_type, NULL, BLE_HS_FOREVER, &params, on_gap_event, NULL);
    if (rc != 0)
        ESP_LOGE(TAG, "BLE advertising failed: %d", rc);
    ble_state = rc == 0 ? RADIO_WORKING : RADIO_OFF;
}

static void on_ble_sync(void) {
    if (ble_hs_util_ensure_addr(0) != 0 || ble_hs_id_infer_auto(0, &own_addr_type) != 0) {
        ESP_LOGE(TAG, "BLE has no usable address");
        return;
    }
    advertise();
}

static void ble_host_task(void* arg) {
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_start(void) {
    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "BLE init failed: %s", esp_err_to_name(err));
        return;
    }
    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("dot");
    ble_hs_cfg.sync_cb = on_ble_sync;
    nimble_port_freertos_init(ble_host_task);
}
#endif

void radio_start(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %s", esp_err_to_name(err));
        return;
    }

#if CONFIG_DOT_WIFI
    wifi_start();
#endif
#if CONFIG_DOT_BLUETOOTH
    ble_start();
#endif
}
