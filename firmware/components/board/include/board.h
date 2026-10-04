#pragma once

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

/* Set the display brightness, 0-100. */
void board_set_brightness(uint8_t percent);
