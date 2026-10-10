#pragma once

/* Wi-Fi and Bluetooth LE. For now they only connect and report their state: the host link is
 * still the USB cable. */

typedef enum {
    RADIO_OFF,       /* disabled, not configured, or failed to start */
    RADIO_WORKING,   /* Wi-Fi: joining the network. BLE: advertising, waiting for a central. */
    RADIO_CONNECTED, /* Wi-Fi: has an IP address. BLE: a central is connected. */
} radio_state_t;

/* Start the radios that are enabled in menuconfig (Dot: Radios). Failures are logged, not fatal. */
void radio_start(void);

radio_state_t radio_wifi_state(void);
radio_state_t radio_ble_state(void);
