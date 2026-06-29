#include "LVGL_Driver.h"

#define TOUCH_H_OFFSET (0)

static const char* LVGL_TAG = "LVGL";

#define DRAW_BUF_SIZE (EXAMPLE_LCD_H_RES * 20 * 2)

static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    esp_lcd_panel_handle_t handle = lv_display_get_user_data(disp);
    esp_lcd_panel_draw_bitmap(handle, area->x1, area->y1, area->x2 + 1, area->y2 + 1, px_map);
    lv_display_flush_ready(disp);
}

static void touchpad_read(lv_indev_t* indev, lv_indev_data_t* data) {
    static uint16_t last_x = 0, last_y = 0;
    static uint8_t release_count = 0;
    static bool is_pressed = false;

    uint16_t x[1] = {0};
    uint16_t y[1] = {0};
    uint8_t cnt = 0;

    esp_lcd_touch_read_data(lv_indev_get_user_data(indev));
    bool touched =
        esp_lcd_touch_get_coordinates(lv_indev_get_user_data(indev), x, y, NULL, &cnt, 1);

    if (touched && cnt > 0) {
        last_x = x[0];
        last_y = y[0];
        release_count = 0;
        is_pressed = true;
    } else {
        /* The CST820 holds INT low for the whole duration of a touch, so reads
         * during contact always see the press; an empty read means the finger
         * has genuinely lifted. Require just 2 consecutive empty reads to filter
         * a stray I2C glitch — more than that only adds release latency, which
         * shows up as delayed taps since buttons fire CLICKED on release. */
        if (is_pressed && ++release_count >= 2) {
            is_pressed = false;
            release_count = 0;
        }
    }

    data->point.x = (int32_t)last_x + TOUCH_H_OFFSET;
    data->point.y = last_y;
    data->state = is_pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

static void lvgl_tick_cb(void* arg) {
    lv_tick_inc(EXAMPLE_LVGL_TICK_PERIOD_MS);
}

void LVGL_Init(void) {
    ESP_LOGI(LVGL_TAG, "Initialize LVGL");
    lv_init();

    ESP_LOGI(LVGL_TAG, "Create display");
    lv_display_t* disp = lv_display_create(EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_user_data(disp, panel_handle);
    lv_display_set_flush_cb(disp, flush_cb);

    ESP_LOGI(LVGL_TAG, "Allocate draw buffers from internal SRAM");
    void* raw1 = heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    void* raw2 = heap_caps_malloc(DRAW_BUF_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(raw1 && raw2);

    static lv_draw_buf_t draw_buf1, draw_buf2;
    lv_draw_buf_init(&draw_buf1, EXAMPLE_LCD_H_RES, 20, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO,
                     raw1, DRAW_BUF_SIZE);
    lv_draw_buf_init(&draw_buf2, EXAMPLE_LCD_H_RES, 20, LV_COLOR_FORMAT_RGB565, LV_STRIDE_AUTO,
                     raw2, DRAW_BUF_SIZE);

    lv_display_set_draw_buffers(disp, &draw_buf1, &draw_buf2);
    lv_display_set_render_mode(disp, LV_DISPLAY_RENDER_MODE_PARTIAL);

    ESP_LOGI(LVGL_TAG, "Register touch input");
    lv_indev_t* indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_display(indev, disp);
    lv_indev_set_read_cb(indev, touchpad_read);
    lv_indev_set_user_data(indev, tp);
    lv_timer_set_period(lv_indev_get_read_timer(indev), 1);
    lv_indev_set_scroll_limit(indev,
                              20); /* raise from default 10 px to reduce mis-classified taps */

    ESP_LOGI(LVGL_TAG, "Install LVGL tick timer");
    const esp_timer_create_args_t tick_timer_args = {.callback = lvgl_tick_cb, .name = "lvgl_tick"};
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, EXAMPLE_LVGL_TICK_PERIOD_MS * 1000));
}
