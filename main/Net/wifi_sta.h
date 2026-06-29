#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Connect (or reconnect) to the given AP in station mode. Safe to call again
 * with new credentials — it reconfigures and reconnects. Non-blocking: use
 * wifi_sta_is_connected() to check link state. */
void wifi_sta_start(const char* ssid, const char* pass);

/* Bring up the SoftAP provisioning hotspot (SSID "CI-Orb-Setup") in AP+STA mode.
 * The config web server is reachable at http://192.168.4.1 while it is up. The
 * AP is dropped automatically once a station connection obtains an IP. */
void wifi_sta_start_ap(void);

/* True once an IP has been obtained. */
bool wifi_sta_is_connected(void);

/* Returns the current IP address string (e.g. "192.168.1.42"), or "" if not connected. */
const char* wifi_sta_get_ip(void);

/* Optional callbacks fired from the WiFi/IP event task (must not block).
 * on_connected fires on IP acquisition; on_disconnected fires on link loss. */
typedef void (*wifi_sta_connected_cb_t)(void);
typedef void (*wifi_sta_disconnected_cb_t)(void);
void wifi_sta_set_callbacks(wifi_sta_connected_cb_t on_connected,
                            wifi_sta_disconnected_cb_t on_disconnected);
