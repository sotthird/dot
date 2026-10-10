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

/* Ease the brightness to `percent` over `ms`. The steps are even to the eye rather than in the
 * raw value. Safe to call while a fade is running: it carries on from where it is. */
void board_fade_brightness(uint8_t percent, uint32_t ms);

/* The brightness the user chose. Idle dimming, fades and rotation return to it. Setting it also
 * applies it, easing over `fade_ms` (0 to jump). */
uint8_t board_user_brightness(void);
void board_set_user_brightness(uint8_t percent, uint32_t fade_ms);

/* True once per short press of the power button, then cleared. Always false on a board whose
 * power button is not readable. */
bool board_power_button_pressed(void);
