#include "status_ui.h"

#include <stdio.h>

#include "board.h"
#include "driver/usb_serial_jtag.h"
#include "lvgl.h"
#include "radio.h"

#define POLL_MS 1000
#define LOW_PERCENT 15

#define COLOR_OFF 0x505050
#define COLOR_WORKING 0xF5A623
#define COLOR_OK 0x2ECC71
#define COLOR_NORMAL 0xB0B0B0
#define COLOR_LOW 0xE74C3C
#define COLOR_BG 0x121212 /* the apps' background, which the row fades into */

static lv_obj_t* usb_icon;
static lv_obj_t* wifi_icon;
static lv_obj_t* ble_icon;
static lv_obj_t* battery_label;

static const char* battery_symbol(int percent) {
    if (percent >= 85)
        return LV_SYMBOL_BATTERY_FULL;
    if (percent >= 60)
        return LV_SYMBOL_BATTERY_3;
    if (percent >= 35)
        return LV_SYMBOL_BATTERY_2;
    if (percent >= LOW_PERCENT)
        return LV_SYMBOL_BATTERY_1;
    return LV_SYMBOL_BATTERY_EMPTY;
}

/* Each icon remembers the colour it should have, so the whole row can fade without losing it.
 * `fade` is 0 when shown and 255 when blended completely into the background. */
#define ITEM_COUNT 4
static struct {
    lv_obj_t* obj;
    uint32_t color;
} items[ITEM_COUNT];
static int32_t fade;

static void paint(unsigned i) {
    lv_obj_set_style_text_color(
        items[i].obj,
        lv_color_mix(lv_color_hex(COLOR_BG), lv_color_hex(items[i].color), (uint8_t)fade), 0);
}

static void set_color(lv_obj_t* obj, uint32_t color) {
    for (unsigned i = 0; i < ITEM_COUNT; i++) {
        if (items[i].obj == obj) {
            items[i].color = color;
            paint(i);
            return;
        }
    }
}

/* The eye sees about twenty shades; each change is a redraw of four icons. */
#define FADE_STEP 12

static void fade_cb(void* var, int32_t v) {
    v = v >= 255 - FADE_STEP / 2 ? 255 : v / FADE_STEP * FADE_STEP;
    if (v == fade)
        return;
    fade = v;
    for (unsigned i = 0; i < ITEM_COUNT; i++)
        if (items[i].obj)
            paint(i);
}

void status_ui_set_hidden(bool hidden, uint32_t ms, uint32_t delay_ms) {
    if (!items[0].obj)
        return; /* no status row on this board */
    lv_anim_delete(&fade, fade_cb);
    if (ms == 0) {
        fade = -1; /* force the repaint */
        fade_cb(NULL, hidden ? 255 : 0);
        return;
    }
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, &fade);
    lv_anim_set_exec_cb(&a, fade_cb);
    lv_anim_set_values(&a, fade, hidden ? 255 : 0);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_delay(&a, delay_ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    lv_anim_start(&a);
}

static uint32_t radio_color(radio_state_t state) {
    return state == RADIO_CONNECTED ? COLOR_OK : state == RADIO_WORKING ? COLOR_WORKING : COLOR_OFF;
}

static lv_obj_t* make_icon(lv_obj_t* row, unsigned index, const char* symbol) {
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, symbol);
    items[index].obj = label;
    set_color(label, COLOR_OFF);
    return label;
}

static void poll_cb(lv_timer_t* timer) {
    set_color(usb_icon, usb_serial_jtag_is_connected() ? COLOR_OK : COLOR_OFF);
    set_color(wifi_icon, radio_color(radio_wifi_state()));
    set_color(ble_icon, radio_color(radio_ble_state()));

    int percent;
    bool charging, usb_power;
    if (!board_battery_read(&percent, &charging, &usb_power) || percent < 0) {
        lv_obj_add_flag(battery_label, LV_OBJ_FLAG_HIDDEN);  // no battery fitted
        return;
    }

    char text[32];
    snprintf(text, sizeof(text), "%s%s %d%%", charging ? LV_SYMBOL_CHARGE " " : "",
             battery_symbol(percent), percent);
    lv_label_set_text(battery_label, text);
    set_color(battery_label, charging                ? COLOR_OK
                             : percent < LOW_PERCENT ? COLOR_LOW
                                                     : COLOR_NORMAL);
    lv_obj_remove_flag(battery_label, LV_OBJ_FLAG_HIDDEN);
}

void status_ui_start(void) {
    /* The top layer sits above whichever app screen is showing. */
    lv_obj_t* row = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(row);
    lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_size(row, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row, 12, 0);
    lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -14);

    usb_icon = make_icon(row, 0, LV_SYMBOL_USB);
    wifi_icon = make_icon(row, 1, LV_SYMBOL_WIFI);
    ble_icon = make_icon(row, 2, LV_SYMBOL_BLUETOOTH);
    battery_label = make_icon(row, 3, "");

    poll_cb(NULL);
    lv_timer_create(poll_cb, POLL_MS, NULL);
}
