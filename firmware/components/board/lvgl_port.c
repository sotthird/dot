#include <assert.h>

#include "board_priv.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#define TICK_PERIOD_MS 2

static const char* TAG = "lvgl";

/* RGB panel: draw_bitmap copies into the framebuffer before returning. */
static void flush_rgb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

/* QSPI panel: the transfer is queued and reads the draw buffer by DMA, so LVGL may
 * only reuse the buffer once the panel IO reports the transfer done. */
static void flush_qspi(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(disp);
    lv_draw_sw_rgb565_swap(px_map, lv_area_get_size(area));
    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
}

static bool qspi_transfer_done(esp_lcd_panel_io_handle_t io, esp_lcd_panel_io_event_data_t* event,
                               void* user_ctx) {
    lv_display_flush_ready(user_ctx);
    return false;
}

/* The panel only accepts windows that start on an even and end on an odd coordinate. */
static void round_area_to_even(lv_event_t* e) {
    lv_area_t* area = lv_event_get_param(e);
    area->x1 &= ~1;
    area->y1 &= ~1;
    area->x2 |= 1;
    area->y2 |= 1;
}

static void touch_read(lv_indev_t* indev, lv_indev_data_t* data) {
    esp_lcd_touch_handle_t touch = lv_indev_get_user_data(indev);
    esp_lcd_touch_point_data_t point;
    uint8_t count = 0;

    /* A failed read leaves the previous point behind in the driver, so treat it as released
     * rather than a finger stuck down. */
    if (esp_lcd_touch_read_data(touch) == ESP_OK &&
        esp_lcd_touch_get_data(touch, &point, &count, 1) == ESP_OK && count > 0) {
        data->point.x = point.x;
        data->point.y = point.y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }

#if BOARD_TOUCH_DEBUG
    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    if (data->state != last_state) {
        if (data->state == LV_INDEV_STATE_PRESSED)
            ESP_LOGI(TAG, "touch down x=%d y=%d", (int)data->point.x, (int)data->point.y);
        else
            ESP_LOGI(TAG, "touch up");
        last_state = data->state;
    }
#endif
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

    if (hw->panel_io) {
        lv_display_set_flush_cb(disp, flush_qspi);
        lv_display_add_event_cb(disp, round_area_to_even, LV_EVENT_INVALIDATE_AREA, NULL);
        const esp_lcd_panel_io_callbacks_t cbs = {.on_color_trans_done = qspi_transfer_done};
        ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(hw->panel_io, &cbs, disp));
    } else {
        lv_display_set_flush_cb(disp, flush_rgb);
    }

    /* Two draw buffers in internal DMA-capable RAM */
    static lv_draw_buf_t draw_buf[2];
    for (int i = 0; i < 2; i++) {
        void* mem = heap_caps_malloc(BOARD_DRAW_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        assert(mem);
        lv_draw_buf_init(&draw_buf[i], BOARD_LCD_H_RES, BOARD_DRAW_BUF_ROWS, LV_COLOR_FORMAT_RGB565,
                         LV_STRIDE_AUTO, mem, BOARD_DRAW_BUF_BYTES);
    }
    lv_display_set_draw_buffers(disp, &draw_buf[0], &draw_buf[1]);

    /* A board without a working touch controller yet simply has no input device */
    if (hw->touch) {
        lv_indev_t* indev = lv_indev_create();
        lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
        lv_indev_set_display(indev, disp);
        lv_indev_set_read_cb(indev, touch_read);
        lv_indev_set_user_data(indev, hw->touch);
        lv_timer_set_period(lv_indev_get_read_timer(indev), BOARD_TOUCH_POLL_MS);
    }

    const esp_timer_create_args_t tick_args = {.callback = tick_cb, .name = "lvgl_tick"};
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "ready (%dx%d, %s)", BOARD_LCD_H_RES, BOARD_LCD_V_RES,
             hw->panel_io ? "QSPI" : "RGB");
}
