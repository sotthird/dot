#include <math.h>

#include "board_priv.h"
#include "lvgl.h"

#if !CONFIG_DOT_BOARD_AMOLED_2_16
/* Only the AMOLED board has a battery chip, and with it a readable power button. */
bool board_battery_read(int* percent, bool* charging, bool* usb_power) {
    return false;
}

bool board_power_button_pressed(void) {
    return false;
}
#endif

#define BRIGHTNESS_UNKNOWN 0xFF
#define FADE_RANGE 1000 /* animation runs in perceptual units 0..FADE_RANGE */

static uint8_t s_shown = BRIGHTNESS_UNKNOWN;
static uint8_t s_user_brightness = BOARD_DEFAULT_BRIGHTNESS;

void board_set_brightness(uint8_t percent) {
    if (percent > 100)
        percent = 100;
    if (percent == s_shown)
        return;
    s_shown = percent;
    board_hw_set_brightness(percent);
}

/* Equal steps of perceived brightness are roughly equal steps of the square root. */
static int32_t percent_to_perceptual(uint8_t percent) {
    return (int32_t)(sqrtf(percent / 100.0f) * FADE_RANGE + 0.5f);
}

static void fade_exec(void* var, int32_t v) {
    const float x = (float)v / FADE_RANGE;
    board_set_brightness((uint8_t)(100.0f * x * x + 0.5f));
}

void board_fade_brightness(uint8_t percent, uint32_t ms) {
    static int fade_token;
    lv_anim_delete(&fade_token, fade_exec);
    if (percent > 100)
        percent = 100;
    if (ms == 0 || s_shown == BRIGHTNESS_UNKNOWN || s_shown == percent) {
        board_set_brightness(percent);
        return;
    }

    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &fade_token);
    lv_anim_set_exec_cb(&a, fade_exec);
    lv_anim_set_values(&a, percent_to_perceptual(s_shown), percent_to_perceptual(percent));
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

uint8_t board_user_brightness(void) {
    return s_user_brightness;
}

void board_set_user_brightness(uint8_t percent, uint32_t fade_ms) {
    s_user_brightness = percent > 100 ? 100 : percent;
    board_fade_brightness(s_user_brightness, fade_ms);
}

void board_init(void) {
    board_hw_t hw = {0};
    board_hw_init(&hw);
    board_lvgl_init(&hw);
}
