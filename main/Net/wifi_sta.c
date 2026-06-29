#include "wifi_sta.h"

#include <stdlib.h>
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

static EventGroupHandle_t s_eg = NULL;
static bool s_started = false;
static bool s_running = false;
static int s_retry = 0;
static wifi_sta_connected_cb_t    s_on_connected    = NULL;
static wifi_sta_disconnected_cb_t s_on_disconnected = NULL;
static char s_ip_str[16] = "";

static void on_event(void* arg, esp_event_base_t base, int32_t id, void* data) {
    (void)arg;
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_ip_str[0] = '\0';
        xEventGroupClearBits(s_eg, CONNECTED_BIT);
        if (s_on_disconnected) s_on_disconnected();
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
        if (s_on_connected) s_on_connected();
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
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_event, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_event,
                                                        NULL, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    s_started = true;
}

void wifi_sta_start(const char* ssid, const char* pass) {
    wifi_init();

    if (s_running) {
        /* Reconfiguring with new credentials — drop the current link first. */
        esp_wifi_stop();
        s_running = false;
        xEventGroupClearBits(s_eg, CONNECTED_BIT);
    }

    wifi_config_t wc = {0};
    strncpy((char*)wc.sta.ssid, ssid ? ssid : "", sizeof(wc.sta.ssid) - 1);
    strncpy((char*)wc.sta.password, pass ? pass : "", sizeof(wc.sta.password) - 1);
    /* Accept open or secured APs; the strongest mode the AP advertises is used. */
    wc.sta.threshold.authmode = WIFI_AUTH_OPEN;

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    s_retry = 0;
    ESP_ERROR_CHECK(esp_wifi_start());
    s_running = true;
}

void wifi_sta_scan(char out_ssids[][33], uint16_t max_count, uint16_t* found_count) {
    *found_count = 0;
    wifi_init();

    bool we_started = false;
    if (!s_running) {
        wifi_config_t wc = {0};
        esp_wifi_set_config(WIFI_IF_STA, &wc);
        if (esp_wifi_start() != ESP_OK)
            return;
        s_running = true;
        we_started = true;
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (esp_wifi_scan_start(NULL, true) != ESP_OK) {
        if (we_started) { esp_wifi_stop(); s_running = false; }
        return;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);

    if (ap_count > 0) {
        uint16_t fetch = ap_count < 32 ? ap_count : 32;
        wifi_ap_record_t* aps = malloc(sizeof(wifi_ap_record_t) * fetch);
        if (aps) {
            esp_wifi_scan_get_ap_records(&fetch, aps);

            /* Insertion sort by RSSI descending. */
            for (uint16_t i = 1; i < fetch; i++) {
                wifi_ap_record_t key = aps[i];
                int j = i - 1;
                while (j >= 0 && aps[j].rssi < key.rssi) {
                    aps[j + 1] = aps[j];
                    j--;
                }
                aps[j + 1] = key;
            }

            /* Deduplicate by SSID and copy to output. */
            uint16_t out = 0;
            for (uint16_t i = 0; i < fetch && out < max_count; i++) {
                const char* ssid = (const char*)aps[i].ssid;
                if (!ssid[0])
                    continue; /* skip hidden networks */
                bool dup = false;
                for (uint16_t k = 0; k < out; k++) {
                    if (strcmp(out_ssids[k], ssid) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) {
                    strncpy(out_ssids[out], ssid, 32);
                    out_ssids[out][32] = '\0';
                    out++;
                }
            }
            *found_count = out;
            free(aps);
        }
    }

    if (we_started) {
        esp_wifi_stop();
        s_running = false;
    }
}

bool wifi_sta_is_connected(void) {
    return s_eg && (xEventGroupGetBits(s_eg) & CONNECTED_BIT);
}

void wifi_sta_set_callbacks(wifi_sta_connected_cb_t on_connected,
                            wifi_sta_disconnected_cb_t on_disconnected) {
    s_on_connected    = on_connected;
    s_on_disconnected = on_disconnected;
}

const char* wifi_sta_get_ip(void) {
    return s_ip_str;
}
