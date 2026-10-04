#include <assert.h>
#include <math.h>

#include "board_priv.h"
#include "esp_check.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#define TICK_PERIOD_MS 2

static const char* TAG = "lvgl";

#if CONFIG_DOT_AUTO_ROTATE
/* The picture is turned in software as each strip is flushed, as Clawdmeter does on this
 * board. LVGL keeps drawing an upright 480x480 screen. */
#define ROTATE_TICK_MS 25    /* timer period: brightness ramp step */
#define ROTATE_POLL_MS 100   /* accelerometer read period */
#define ROTATE_STABLE_MS 300 /* a new orientation must hold this long */
#define ROTATE_TILT_MIN 0.5f /* ignore tilts under ~30 degrees from flat */
#define ROTATE_AMBIGUOUS 255

static uint8_t s_rotation; /* quarter turns clockwise from the default mounting, 0-3 */
static uint16_t* s_rot_buf; /* DMA-capable; holds one rotated strip */
static esp_err_t (*s_read_accel)(float* ax, float* ay);

/* Turn a strip into s_rot_buf (byte-swapped for the panel) and return where it lands. */
static void rotate_area(const uint16_t* src, const lv_area_t* a, lv_area_t* dst) {
    const int S = BOARD_LCD_H_RES;
    const int w = lv_area_get_width(a);
    const int h = lv_area_get_height(a);

    switch (s_rotation) {
    case 1: /* 90 CW: (x,y) -> (S-1-y, x) */
        dst->x1 = S - a->y1 - h;
        dst->y1 = a->x1;
        dst->x2 = dst->x1 + h - 1;
        dst->y2 = dst->y1 + w - 1;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                s_rot_buf[x * h + (h - 1 - y)] = __builtin_bswap16(src[y * w + x]);
        break;
    case 2: /* 180: (x,y) -> (S-1-x, S-1-y) */
        dst->x1 = S - a->x1 - w;
        dst->y1 = S - a->y1 - h;
        dst->x2 = dst->x1 + w - 1;
        dst->y2 = dst->y1 + h - 1;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                s_rot_buf[(h - 1 - y) * w + (w - 1 - x)] = __builtin_bswap16(src[y * w + x]);
        break;
    default: /* 270 CW: (x,y) -> (y, S-1-x) */
        dst->x1 = a->y1;
        dst->y1 = S - a->x1 - w;
        dst->x2 = dst->x1 + h - 1;
        dst->y2 = dst->y1 + w - 1;
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                s_rot_buf[(w - 1 - x) * h + y] = __builtin_bswap16(src[y * w + x]);
        break;
    }
}

/* Touch reports where the finger is on the panel; map it back to where LVGL drew it. */
static void unrotate_point(int32_t* x, int32_t* y) {
    const int32_t max = BOARD_LCD_H_RES - 1;
    int32_t px = *x < 0 ? 0 : *x > max ? max : *x;
    int32_t py = *y < 0 ? 0 : *y > max ? max : *y;

    switch (s_rotation) {
    case 1:
        *x = py;
        *y = max - px;
        break;
    case 2:
        *x = max - px;
        *y = max - py;
        break;
    case 3:
        *x = max - py;
        *y = px;
        break;
    default:
        *x = px;
        *y = py;
        break;
    }
}

static uint8_t accel_to_rotation(float ax, float ay) {
    const float abs_ax = fabsf(ax);
    const float abs_ay = fabsf(ay);
    if (abs_ax < ROTATE_TILT_MIN && abs_ay < ROTATE_TILT_MIN)
        return ROTATE_AMBIGUOUS; /* lying flat */
    if (abs_ay > abs_ax)
        return ay > 0 ? 3 : 1;
    return ax > 0 ? 0 : 2;
}

/* Blank on a change, then bring the brightness back in steps once the new picture is drawn */
static void brightness_ramp(uint8_t* step, uint8_t* wait) {
    static const uint8_t pct[] = {30, 60, 85, 100};
    if (*step >= sizeof(pct))
        return;
    if (*wait) {
        (*wait)--;
        return;
    }
    board_set_brightness(BOARD_DEFAULT_BRIGHTNESS * pct[*step] / 100);
    (*step)++;
}

static void rotate_timer_cb(lv_timer_t* timer) {
    static uint8_t ramp_step = 4; /* idle */
    static uint8_t ramp_wait;
    static uint8_t candidate;
    static uint32_t candidate_since;
    static uint32_t since_poll;

    brightness_ramp(&ramp_step, &ramp_wait);

    since_poll += ROTATE_TICK_MS;
    if (since_poll < ROTATE_POLL_MS)
        return;
    since_poll = 0;

    float ax, ay;
    if (s_read_accel(&ax, &ay) != ESP_OK)
        return;

#if BOARD_ROTATE_DEBUG
    static uint8_t debug_n;
    if (++debug_n >= 10) {
        debug_n = 0;
        ESP_LOGI(TAG, "accel ax=%.2f ay=%.2f -> %d (now %d)", ax, ay, accel_to_rotation(ax, ay),
                 s_rotation);
    }
#endif

    const uint8_t target = accel_to_rotation(ax, ay);
    if (target == ROTATE_AMBIGUOUS || target == s_rotation) {
        candidate = s_rotation;
        return;
    }
    const uint32_t now = lv_tick_get();
    if (target != candidate) {
        candidate = target;
        candidate_since = now;
    } else if (now - candidate_since >= ROTATE_STABLE_MS) {
        s_rotation = target;
        ESP_LOGI(TAG, "rotation %d (ax=%.2f ay=%.2f)", s_rotation, ax, ay);
        board_set_brightness(0);
        lv_obj_invalidate(lv_screen_active());
        ramp_step = 0;
        ramp_wait = 4;
    }
}
#endif /* CONFIG_DOT_AUTO_ROTATE */

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
#if CONFIG_DOT_AUTO_ROTATE
    if (s_rotation) {
        lv_area_t dst;
        rotate_area((const uint16_t*)px_map, area, &dst);
        esp_lcd_panel_draw_bitmap(panel, dst.x1, dst.y1, dst.x2 + 1, dst.y2 + 1, s_rot_buf);
        return;
    }
#endif
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
#if CONFIG_DOT_AUTO_ROTATE
        unrotate_point(&data->point.x, &data->point.y);
#endif
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

#if CONFIG_DOT_AUTO_ROTATE
    if (hw->read_accel && hw->panel_io) {
        s_rot_buf = heap_caps_malloc(BOARD_DRAW_BUF_BYTES, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
        assert(s_rot_buf);
        s_read_accel = hw->read_accel;
        lv_timer_create(rotate_timer_cb, ROTATE_TICK_MS, NULL);
    }
#endif

    const esp_timer_create_args_t tick_args = {.callback = tick_cb, .name = "lvgl_tick"};
    esp_timer_handle_t tick_timer;
    ESP_ERROR_CHECK(esp_timer_create(&tick_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, TICK_PERIOD_MS * 1000));

    ESP_LOGI(TAG, "ready (%dx%d, %s)", BOARD_LCD_H_RES, BOARD_LCD_V_RES,
             hw->panel_io ? "QSPI" : "RGB");
}
