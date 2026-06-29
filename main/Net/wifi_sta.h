#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Connect (or reconnect) to the given AP in station mode. Safe to call again
 * with new credentials — it reconfigures and reconnects. Non-blocking: use
 * wifi_sta_is_connected() to check link state. */
void wifi_sta_start(const char* ssid, const char* pass);

/* True once an IP has been obtained. */
bool wifi_sta_is_connected(void);

/* Returns the current IP address string (e.g. "192.168.1.42"), or "" if not connected. */
const char* wifi_sta_get_ip(void);

/* Blocking WiFi scan. Initialises the WiFi driver if needed (safe to call
 * before wifi_sta_start). Writes up to max_count unique SSIDs sorted by
 * signal strength into out_ssids[i] (each 33 bytes, NUL-terminated). Sets
 * *found_count to the number of entries written. */
void wifi_sta_scan(char out_ssids[][33], uint16_t max_count, uint16_t* found_count);

/* Optional callbacks fired from the WiFi/IP event task (must not block).
 * on_connected fires on IP acquisition; on_disconnected fires on link loss. */
typedef void (*wifi_sta_connected_cb_t)(void);
typedef void (*wifi_sta_disconnected_cb_t)(void);
void wifi_sta_set_callbacks(wifi_sta_connected_cb_t on_connected,
                            wifi_sta_disconnected_cb_t on_disconnected);
