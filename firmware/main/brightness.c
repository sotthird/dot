#include "brightness.h"

#include <stdio.h>
#include <stdlib.h>

#include "board.h"
#include "esp_log.h"
#include "lvgl.h"
#include "nvs.h"
#include "nvs_flash.h"

static const char* TAG = "brightness";

#define POLL_MS 50
#define SHOW_MS 1500
#define FADE_MS 450
#define NVS_NAMESPACE "dot"
#define NVS_KEY "brightness"

/* Percent, dimmest to brightest. The AMOLED is very bright, so the low steps are the useful ones.
 */
static const uint8_t levels[] = {5, 15, 30, 60, 100};
#define LEVEL_COUNT (sizeof(levels) / sizeof(levels[0]))

static unsigned level_index;
static lv_obj_t* popup;
static lv_timer_t* hide_timer;

static unsigned nearest_level(uint8_t percent) {
    unsigned best = 0;
    for (unsigned i = 1; i < LEVEL_COUNT; i++)
        if (abs((int)levels[i] - percent) < abs((int)levels[best] - percent))
            best = i;
    return best;
}

static void save(void) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs) != ESP_OK)
        return;
    nvs_set_u8(nvs, NVS_KEY, levels[level_index]);
    nvs_commit(nvs);
    nvs_close(nvs);
}

static bool load(uint8_t* percent) {
    nvs_handle_t nvs;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs) != ESP_OK)
        return false;
    esp_err_t err = nvs_get_u8(nvs, NVS_KEY, percent);
    nvs_close(nvs);
    return err == ESP_OK;
}

static void hide_cb(lv_timer_t* timer) {
    hide_timer = NULL; /* one-shot: LVGL deletes it after this callback */
    lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);
}

static void show_popup(void) {
    char text[32];
    snprintf(text, sizeof(text), LV_SYMBOL_EYE_OPEN "  Brightness %d%%", levels[level_index]);
    lv_label_set_text(popup, text);
    lv_obj_remove_flag(popup, LV_OBJ_FLAG_HIDDEN);
    if (hide_timer)
        lv_timer_delete(hide_timer);
    hide_timer = lv_timer_create(hide_cb, SHOW_MS, NULL);
    lv_timer_set_repeat_count(hide_timer, 1);
}

static void poll_cb(lv_timer_t* timer) {
    if (!board_power_button_pressed())
        return;

    level_index = (level_index + 1) % LEVEL_COUNT;
    board_set_user_brightness(levels[level_index], FADE_MS);
    /* Pressing the button counts as activity, so an idle-dimmed screen does not dim again at once.
     */
    lv_display_trigger_activity(NULL);
    save();
    show_popup();
    ESP_LOGI(TAG, "%d%%", levels[level_index]);
}

void brightness_start(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    uint8_t saved;
    level_index = nearest_level(err == ESP_OK && load(&saved) ? saved : board_user_brightness());
    board_set_user_brightness(levels[level_index], 0);

    /* On the top layer, so it shows over whichever app is on screen. */
    popup = lv_label_create(lv_layer_top());
    lv_obj_remove_flag(popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_text_color(popup, lv_color_white(), 0);
    lv_obj_set_style_bg_color(popup, lv_color_hex(0x202020), 0);
    lv_obj_set_style_bg_opa(popup, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(popup, 14, 0);
    lv_obj_set_style_pad_hor(popup, 18, 0);
    lv_obj_set_style_pad_ver(popup, 10, 0);
    lv_obj_align(popup, LV_ALIGN_BOTTOM_MID, 0, -64);
    lv_obj_add_flag(popup, LV_OBJ_FLAG_HIDDEN);

    lv_timer_create(poll_cb, POLL_MS, NULL);
}
