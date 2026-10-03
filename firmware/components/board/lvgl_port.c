#include <assert.h>

#include "board_priv.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#define TICK_PERIOD_MS 2
#define TOUCH_POLL_MS 2

static const char* TAG = "lvgl";

/* draw_bitmap copies into the RGB panel's framebuffer, so the flush is done on return. */
static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void touch_read(lv_indev_t* indev, lv_indev_data_t* data) {
    esp_lcd_touch_handle_t touch = lv_indev_get_user_data(indev);
    esp_lcd_touch_point_data_t point;
    uint8_t count = 0;

    esp_lcd_touch_read_data(touch);
    if (esp_lcd_touch_get_data(touch, &point, &count, 1) == ESP_OK && count > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void tick_cb(void* arg) {
    lv_tick_inc(TICK_PERIOD_MS);
}

void board_lvgl_init(const board_hw_t* hw) {
    lv_init();

    lv_display_t* disp = lv_display_create(BOARD_LCD_H_RES, BOARD_LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, hw->panel);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);

    lv_display_set_flush_cb(disp, flush_cb);

    /* Two draw buffers in internal DMA-capable RAM */
    static lv_draw_buf_t draw_buf[2];
    for (int i = 0; i < 2; i++) {
        void* mem = heap_caps_malloc(BOARD_DRAW_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        assert(mem);
        lv_draw_buf_init(&draw_buf[i], BOARD_LCD_H_RES, BOARD_DRAW_BUF_ROWS, LV_COLOR_FORMAT_RGB565,
                         LV_STRIDE_AUTO, mem, BOARD_DRAW_BUF_BYTES);
    }
    lv_display_set_draw_buffers(disp, &draw_buf[0], &draw_buf[1]);

    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, disp);
    lv_indev_set_read_cb(indev, touch_read);
    lv_indev_set_user_data(indev, hw->touch);
    lv_timer_set_period(lv_indev_get_read_timer(indev), TOUCH_POLL_MS);

    const esp_timer_create_args_t tick_args = {.callback = tick_cb, .name = "lvgl_tick"};
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "ready (%dx%d)", BOARD_LCD_H_RES, BOARD_LCD_V_RES);
}
