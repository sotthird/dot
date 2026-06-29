#include "wifi_sta.h"

#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define TAG "wifi_sta"
#define CONNECTED_BIT BIT0
#define MAX_RETRY 10

#define AP_SSID "CI-Orb-Setup"
#define AP_PASS "ciorbsetup" /* >=8 chars for WPA2; shown on the device screen */

static EventGroupHandle_t s_eg = NULL;
static bool s_started = false;
static bool s_running = false;
static bool s_ap_active = false; /* SoftAP provisioning hotspot is up */
static int s_retry = 0;
static wifi_sta_connected_cb_t s_on_connected = NULL;
static wifi_sta_disconnected_cb_t s_on_disconnected = NULL;
static char s_ip_str[16] = "";

static void on_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ip_str[0] = '\0';
        xEventGroupClearBits(s_eg, CONNECTED_BIT);
        if (s_on_disconnected)
            s_on_disconnected();
        if (s_retry++ < MAX_RETRY) {
            vTaskDelay(pdMS_TO_TICKS(2000));
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "giving up after %d retries", MAX_RETRY);
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        s_retry = 0;
        xEventGroupSetBits(s_eg, CONNECTED_BIT);
        ESP_LOGI(TAG, "connected, IP: %s", s_ip_str);
        if (s_ap_active) {
            /* Provisioning succeeded — tear down the hotspot. */
            esp_wifi_set_mode(WIFI_MODE_STA);
            s_ap_active = false;
        }
        if (s_on_connected)
            s_on_connected();
    }
}

static void wifi_init(void) {
    if (s_started)
        return;
    if (!s_eg)
        s_eg = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap(); /* used by SoftAP provisioning */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, NULL, NULL));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    s_started = true;
}

void wifi_sta_start(const char* ssid, const char* pass) {
    wifi_init();

    wifi_config_t wc = {0};
    strncpy((char*)wc.sta.ssid, ssid ? ssid : "", sizeof(wc.sta.ssid) - 1);
    strncpy((char*)wc.sta.password, pass ? pass : "", sizeof(wc.sta.password) - 1);
    /* Accept open or secured APs; the strongest mode the AP advertises is used. */
    wc.sta.threshold.authmode = WIFI_AUTH_OPEN;

    if (s_ap_active) {
        /* Provisioning handover: keep the AP up (APSTA) so the phone still
         * receives the POST response. Switching to APSTA brings the STA
         * interface up, which auto-connects with the config we just set. The AP
         * is dropped in the GOT_IP handler once the link is established. */
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
        s_retry = 0;
        ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
        s_running = true;
        return;
    }

    if (s_running) {
        /* Reconfiguring with new credentials — drop the current link first. */
        esp_wifi_stop();
        s_running = false;
        xEventGroupClearBits(s_eg, CONNECTED_BIT);
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    s_retry = 0;
    ESP_ERROR_CHECK(esp_wifi_start());
    s_running = true;
}

void wifi_sta_start_ap(void) {
    wifi_init();
    if (s_ap_active)
        return;

    wifi_config_t ap = {0};
    strncpy((char*)ap.ap.ssid, AP_SSID, sizeof(ap.ap.ssid) - 1);
    ap.ap.ssid_len = strlen(AP_SSID);
    strncpy((char*)ap.ap.password, AP_PASS, sizeof(ap.ap.password) - 1);
    ap.ap.channel = 1;
    ap.ap.max_connection = 4;
    ap.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(s_running ? WIFI_MODE_APSTA : WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
    if (!s_running) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_running = true;
    }
    s_ap_active = true;
    ESP_LOGI(TAG, "SoftAP up: %s", AP_SSID);
}

bool wifi_sta_is_connected(void) {
    return s_eg && (xEventGroupGetBits(s_eg) & CONNECTED_BIT);
}

void wifi_sta_set_callbacks(wifi_sta_connected_cb_t on_connected,
                            wifi_sta_disconnected_cb_t on_disconnected) {
    s_on_connected = on_connected;
    s_on_disconnected = on_disconnected;
}

const char* wifi_sta_get_ip(void) {
    return s_ip_str;
}
