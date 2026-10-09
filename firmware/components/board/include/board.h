#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "sdkconfig.h"

#if CONFIG_DOT_BOARD_AMOLED_2_16
#define BOARD_LCD_H_RES 480
#define BOARD_LCD_V_RES 480
#else
#define BOARD_LCD_H_RES 480
#define BOARD_LCD_V_RES 480
#endif

/* Bring up the display, touch controller and LVGL (tick timer and input device).
 * Call once at startup; LVGL is then ready for screens to be created. */
void board_init(void);

/* An AMOLED is much brighter than the LCD at the same setting */
#if CONFIG_DOT_BOARD_AMOLED_2_16
#define BOARD_DEFAULT_BRIGHTNESS 30
#else
#define BOARD_DEFAULT_BRIGHTNESS 70
#endif

/* Battery state. Returns false on a board without a power management chip. `percent` is -1 when
 * no battery is connected; `usb_power` is true while a cable supplies power. */
bool board_battery_read(int* percent, bool* charging, bool* usb_power);

/* Set the display brightness, 0-100. */
void board_set_brightness(uint8_t percent);
