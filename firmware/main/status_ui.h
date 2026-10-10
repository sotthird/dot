#pragma once

#include <stdbool.h>
#include <stdint.h>

/* A row of status icons at the bottom of every screen: USB, Wi-Fi, Bluetooth and the battery.
 * Call after the app screens exist. Meant for a board with a battery chip. */
void status_ui_start(void);

/* Fade the row into the background, or bring it back, over `ms` after `delay_ms` (`ms` of 0 does
 * it at once). Does nothing on a board without the row. */
void status_ui_set_hidden(bool hidden, uint32_t ms, uint32_t delay_ms);
