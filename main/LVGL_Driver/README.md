# LVGL_Driver

Glue layer that wires **LVGL v9** up to the ST7701S display and CST820 touch
controller — display flushing, touch input polling, and the LVGL tick.

## What it does

- Creates the LVGL display object at 480×480 in `RGB565` format and points its
  flush callback at `esp_lcd_panel_draw_bitmap()`.
- Allocates two small partial draw buffers from **internal SRAM with DMA
  capability** (`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`) — PSRAM was tried first but
  is too slow for the rendering rate this display needs, so internal SRAM is used
  with `LV_DISPLAY_RENDER_MODE_PARTIAL` (20 rows per buffer at a time).
- Registers an input device that reads touch coordinates from the CST820 driver
  via the `esp_lcd_touch` abstraction.
- Installs a periodic `esp_timer` that calls `lv_tick_inc()` to drive LVGL's
  internal clock.

## Public API

| Function | Description |
|---|---|
| `void LVGL_Init(void)` | Sets up the LVGL display, draw buffers, touch input device, and tick timer. Call once at startup, after `LCD_Init()`/`Touch_Init()`. |

## Settings / configurable values

| Define | Value | Meaning |
|---|---|---|
| `EXAMPLE_LVGL_TICK_PERIOD_MS` | `2` | Period of the LVGL tick timer (drives animations/timeouts) |
| `DRAW_BUF_SIZE` | `EXAMPLE_LCD_H_RES * 20 * 2` bytes | Size of each partial draw buffer (20 rows × RGB565) |
| Touch read timer period | `2 ms` | Set via `lv_timer_set_period(lv_indev_get_read_timer(indev), 2)` |

## Notes

- **Why internal SRAM, not PSRAM:** PSRAM access was the original cause of a Task
  Watchdog Timeout — the render loop couldn't keep up. Internal SRAM with DMA
  fixed it; the tradeoff is the smaller buffer (only 20 rows), which is why
  `LV_DISPLAY_RENDER_MODE_PARTIAL` is used instead of full-frame buffering.
- The touch read callback checks `esp_lcd_touch_read_data()` /
  `esp_lcd_touch_get_coordinates()` and reports `LV_INDEV_STATE_PRESSED` /
  `RELEASED` based on whether a finger is detected.
- This module owns no FreeRTOS task of its own — `lv_timer_handler()` must be
  called periodically by the application (see `lvgl_task` in `main.c`), normally
  under a mutex shared with anything else that touches LVGL objects.
