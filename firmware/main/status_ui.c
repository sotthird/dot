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

static void set_color(lv_obj_t* obj, uint32_t color) {
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
}

static uint32_t radio_color(radio_state_t state) {
    return state == RADIO_CONNECTED ? COLOR_OK : state == RADIO_WORKING ? COLOR_WORKING : COLOR_OFF;
}

static lv_obj_t* make_icon(lv_obj_t* row, const char* symbol) {
    lv_obj_t* label = lv_label_create(row);
    lv_label_set_text(label, symbol);
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

    usb_icon = make_icon(row, LV_SYMBOL_USB);
    wifi_icon = make_icon(row, LV_SYMBOL_WIFI);
    ble_icon = make_icon(row, LV_SYMBOL_BLUETOOTH);
    battery_label = make_icon(row, "");

    poll_cb(NULL);
    lv_timer_create(poll_cb, POLL_MS, NULL);
}
