#pragma once

#include <stdint.h>

#include "board.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch.h"

/* LVGL renders in partial mode into two draw buffers of this many full-width rows. */
#define BOARD_DRAW_BUF_ROWS 20
#define BOARD_DRAW_BUF_BYTES (BOARD_LCD_H_RES * BOARD_DRAW_BUF_ROWS * 2)

/* An AMOLED is much brighter than the LCD at the same setting */
#if CONFIG_DOT_BOARD_AMOLED_2_16
#define BOARD_DEFAULT_BRIGHTNESS 30
#else
#define BOARD_DEFAULT_BRIGHTNESS 70
#endif

/* What a board hands to the shared LVGL glue. */
typedef struct {
    esp_lcd_panel_handle_t panel;
    /* Set for QSPI panels, which DMA from the draw buffer: the flush then completes
     * from this IO's transfer-done callback, and pixels go out big-endian. Leave NULL
     * for RGB panels, which copy into their framebuffer synchronously. */
    esp_lcd_panel_io_handle_t panel_io;
    esp_lcd_touch_handle_t touch;
} board_hw_t;

/* Implemented by each board: reset and configure the panel and touch controller. */
void board_hw_init(board_hw_t* hw);

/* Implemented in lvgl_port.c. */
void board_lvgl_init(const board_hw_t* hw);
