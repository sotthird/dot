#include "app.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "board.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "serial_link.h"

#define APP_MAX 8
#define IMAGE_BUF_BYTES (180 * 180 * 2)
#define CMD_DEBOUNCE_MS 500
#define FADE_STEP_MS 25
#define DOT_SIZE 8
#define DOT_ACTIVE_HEIGHT 20
#define DOT_SPACING 20
#define DOT_MARGIN_LEFT 12
#define SWIPE_MIN_PX 60
#define SWIPE_POLL_MS 10

static const char* TAG = "app";

static const app_t* apps[APP_MAX];
static lv_obj_t* screens[APP_MAX];
static int app_count;
/* Written by the LVGL task, read by the serial task. */
static volatile int active;
static SemaphoreHandle_t mutex;
static uint8_t* image_buf;

void app_init(void) {
    mutex = xSemaphoreCreateMutex();
    assert(mutex);

    image_buf = heap_caps_malloc(IMAGE_BUF_BYTES, MALLOC_CAP_SPIRAM);
    assert(image_buf);
}

void app_register(const app_t* app) {
    assert(app_count < APP_MAX);
    apps[app_count++] = app;
}

/* Tell the host which app is on screen so it polls only that one. */
static void announce_active(void) {
    char msg[32];
    snprintf(msg, sizeof(msg), "APP:%s\n", apps[active]->name);
    serial_link_send(msg);
}

/* Switching apps fades the screen through black instead of sliding: redrawing the whole
 * screen every frame is more than this panel can do smoothly, while changing the brightness
 * costs one small command per step. The new screen is loaded while the panel is dark. */
static const uint8_t fade_out_pct[] = {70, 40, 15, 0};
static const uint8_t fade_in_pct[] = {0, 15, 40, 70, 100};
#define FADE_OUT_STEPS (sizeof(fade_out_pct) / sizeof(fade_out_pct[0]))
#define FADE_IN_STEPS (sizeof(fade_in_pct) / sizeof(fade_in_pct[0]))

static lv_timer_t* fade_timer;
static unsigned fade_step;

static void fade_cb(lv_timer_t* timer) {
    if (fade_step < FADE_OUT_STEPS) {
        board_set_brightness(BOARD_DEFAULT_BRIGHTNESS * fade_out_pct[fade_step] / 100);
    } else {
        unsigned i = fade_step - FADE_OUT_STEPS;
        if (i == 0)
            lv_screen_load(screens[active]);
        board_set_brightness(BOARD_DEFAULT_BRIGHTNESS * fade_in_pct[i] / 100);
        if (i + 1 == FADE_IN_STEPS) {
            lv_timer_delete(timer);
            fade_timer = NULL;
            return;
        }
    }
    fade_step++;
}

/* Put the app `step` places along from the active one (wrapping) on screen and the old one on
 * standby. */
static void switch_app(int step) {
    int next = (active + step + app_count) % app_count;
    if (next == active || fade_timer)
        return;

    if (apps[active]->suspend)
        apps[active]->suspend();
    active = next;
    if (apps[active]->resume)
        apps[active]->resume();

    ESP_LOGI(TAG, "app %s", apps[active]->name);
    fade_step = 0;
    fade_timer = lv_timer_create(fade_cb, FADE_STEP_MS, NULL);
    announce_active();
}

/* A swipe is decided here rather than from LVGL's gesture event, which ignores slow or
 * short strokes. Polling the pointer also lets us cancel the press before a button under the finger
 * sees a click. */
static void swipe_timer_cb(lv_timer_t* timer) {
    static lv_point_t start;
    static bool tracking;
    lv_indev_t* indev = lv_timer_get_user_data(timer);

    if (lv_indev_get_state(indev) != LV_INDEV_STATE_PRESSED) {
        tracking = false;
        return;
    }

    lv_point_t now;
    lv_indev_get_point(indev, &now);
    if (!tracking) {
        tracking = true;
        start = now;
        return;
    }

    int dx = now.x - start.x;
    int dy = now.y - start.y;
    if (abs(dx) < SWIPE_MIN_PX || abs(dx) < 2 * abs(dy))
        return;

    tracking = false;
    lv_indev_wait_release(indev);
    switch_app(dx < 0 ? 1 : -1);
}

static void start_swipe_detection(void) {
    for (lv_indev_t* indev = lv_indev_get_next(NULL); indev; indev = lv_indev_get_next(indev))
        if (lv_indev_get_type(indev) == LV_INDEV_TYPE_POINTER)
            lv_timer_create(swipe_timer_cb, SWIPE_POLL_MS, indev);
}

/* A column of dots down the left edge, one per app, with the one for `index` lit. Every screen
 * gets its own, so nothing has to be updated when the app changes. */
static void create_page_dots(lv_obj_t* screen, int index) {
    for (int i = 0; i < app_count; i++) {
        const bool lit = (i == index);
        lv_obj_t* dot = lv_obj_create(screen);
        lv_obj_remove_style_all(dot);
        lv_obj_remove_flag(dot, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(dot, DOT_SIZE, lit ? DOT_ACTIVE_HEIGHT : DOT_SIZE);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(dot, lv_color_hex(lit ? 0xFFFFFF : 0x606060), 0);
        lv_obj_align(dot, LV_ALIGN_LEFT_MID, DOT_MARGIN_LEFT,
                     (2 * i - (app_count - 1)) * DOT_SPACING / 2);
    }
}

void app_create_uis(void) {
    for (int i = 0; i < app_count; i++) {
        lv_obj_t* screen = lv_obj_create(NULL);
        lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
        screens[i] = screen;
        if (apps[i]->create_ui)
            apps[i]->create_ui(screen);
        if (app_count > 1)
            create_page_dots(screen, i);
    }
    if (app_count > 0)
        lv_screen_load(screens[0]);
    start_swipe_detection();
}

void app_update_active(void) {
    if (app_count > 0 && apps[active]->update)
        apps[active]->update();
}

void app_handle_line(const char* line) {
    if (app_count == 0)
        return;

    /* The host asks which app is on screen when it starts. */
    if (strcmp(line, "HELLO") == 0) {
        announce_active();
        return;
    }

    const app_t* app = apps[active];
    if (app->parse && strncmp(line, app->prefix, strlen(app->prefix)) == 0)
        app->parse(line);
}

void app_handle_image(const uint8_t* data, int w, int h, uint32_t color) {
    if (app_count > 0 && apps[active]->image)
        apps[active]->image(data, w, h, color);
}

uint8_t* app_image_buffer(size_t needed) {
    return needed <= IMAGE_BUF_BYTES ? image_buf : NULL;
}

void app_lock(void) {
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void app_unlock(void) {
    xSemaphoreGive(mutex);
}

void app_send_cmd(const char* cmd) {
    static uint32_t last_ms;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_ms < CMD_DEBOUNCE_MS)
        return;
    last_ms = now;

    char msg[32];
    snprintf(msg, sizeof(msg), "CMD:%s\n", cmd);
    serial_link_send(msg);
}
