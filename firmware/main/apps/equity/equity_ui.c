#include "equity_ui.h"

#include <stdio.h>

#include "esp_log.h"

static const char* TAG = "equity";

#define COLOR_BG 0x121212
#define COLOR_TITLE 0xB3B3B3
#define COLOR_GAIN 0x2ECC71
#define COLOR_LOSS 0xE74C3C
#define COLOR_NEUTRAL 0x808080
#define COLOR_WARN 0xF5A623
#define STALE_AGE_S (15 * 60)
#define AMOUNTS_SHOW_MS 5000

/* After this long without a touch the title fades away and the figures settle in larger. Nothing
 * here may draw through an off-screen layer (opacity or scaling of a group): LVGL's memory pool
 * here is too small for one and the UI task then hangs. Colours and positions are safe. */
#define IDLE_MS 20000
#define IDLE_POLL_MS 500
#define ENTER_MS 900
#define EXIT_MS 300
#define SWAP_OUT_MS 250
#define SWAP_IN_MS 350
#define GLIDE_PX 14

static lv_obj_t* title_label;
static lv_obj_t* metrics; /* arrow, total return, unrealized return and position count */
static lv_obj_t* arrow_label;
static lv_obj_t* total_label;
static lv_obj_t* unrealized_label;
static lv_obj_t* positions_label;
static lv_obj_t* status_label;
static lv_obj_t* value_label;
static lv_obj_t* gain_label;
static lv_timer_t* hide_timer;
static lv_timer_t* idle_timer;
static equity_ui_cmd_cb_t cmd_cb;

static bool ambient;

static void tapped_cb(lv_event_t* e) {
    if (cmd_cb)
        cmd_cb("eq_reveal");
}

static lv_obj_t* make_label(lv_obj_t* parent, const char* text, uint32_t color) {
    lv_obj_t* label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static lv_color_t mix_bg(uint32_t color, int32_t v) {
    return lv_color_mix(lv_color_hex(COLOR_BG), lv_color_hex(color), (uint8_t)v);
}

static void title_fade_cb(void* obj, int32_t v) {
    lv_obj_set_style_text_color(obj, mix_bg(COLOR_TITLE, v), 0);
}

static void glide_cb(void* obj, int32_t v) {
    lv_obj_set_style_translate_y(obj, v, 0);
}

static void secondary_fade_cb(void* obj, int32_t v) {
    lv_obj_set_style_text_color(unrealized_label, mix_bg(COLOR_TITLE, v), 0);
    lv_obj_set_style_text_color(positions_label, mix_bg(COLOR_NEUTRAL, v), 0);
}

static void set_secondary_fonts(bool large) {
#if LV_FONT_MONTSERRAT_24 && LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(unrealized_label,
                               large ? &lv_font_montserrat_24 : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(positions_label,
                               large ? &lv_font_montserrat_20 : &lv_font_montserrat_16, 0);
#endif
}

static void animate(void* obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                    lv_anim_completed_cb_t done) {
    lv_anim_delete(obj, cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_path_cb(&a, lv_anim_path_ease_in_out);
    if (done)
        lv_anim_set_completed_cb(&a, done);
    lv_anim_start(&a);
}

/* The two lines under the percentage change size: fade out, swap the font, fade back in. */
static void swap_in_cb(lv_anim_t* a) {
    set_secondary_fonts(ambient);
    animate(unrealized_label, secondary_fade_cb, 255, 0, SWAP_IN_MS, NULL);
}

static void set_ambient(bool on, bool animated) {
    ESP_LOGI(TAG, "ambient %s%s", on ? "on" : "off", animated ? "" : " (reset)");
    ambient = on;
    if (animated) {
        const uint32_t ms = on ? ENTER_MS : EXIT_MS;
        animate(title_label, title_fade_cb, on ? 0 : 255, on ? 255 : 0, ms, NULL);
        animate(metrics, glide_cb, on ? 0 : GLIDE_PX, on ? GLIDE_PX : 0, ms, NULL);
        animate(unrealized_label, secondary_fade_cb, 0, 255, SWAP_OUT_MS, swap_in_cb);
    } else {
        lv_anim_delete(title_label, title_fade_cb);
        lv_anim_delete(metrics, glide_cb);
        lv_anim_delete(unrealized_label, secondary_fade_cb);
        title_fade_cb(title_label, on ? 255 : 0);
        glide_cb(metrics, on ? GLIDE_PX : 0);
        secondary_fade_cb(NULL, 0);
        set_secondary_fonts(on);
    }
}

static void idle_cb(lv_timer_t* timer) {
    const bool idle = lv_display_get_inactive_time(NULL) >= IDLE_MS;
    if (idle != ambient)
        set_ambient(idle, true);
}

void equity_ui_create(lv_obj_t* scr, equity_ui_cmd_cb_t on_cmd) {
    cmd_cb = on_cmd;

    lv_obj_set_style_bg_color(scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_add_event_cb(scr, tapped_cb, LV_EVENT_CLICKED, NULL);

    title_label = make_label(scr, "Portfolio", COLOR_TITLE);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 34);

    /* One container so the figures glide together. */
    metrics = lv_obj_create(scr);
    lv_obj_remove_style_all(metrics);
    lv_obj_remove_flag(metrics, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(metrics, 420, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(metrics, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(metrics, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(metrics, 4, 0);
    lv_obj_align(metrics, LV_ALIGN_CENTER, 0, -22);

    arrow_label = make_label(metrics, "", COLOR_NEUTRAL);
    total_label = make_label(metrics, "---", COLOR_NEUTRAL);
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_font(total_label, &lv_font_montserrat_48, 0);
#endif
    unrealized_label = make_label(metrics, "", COLOR_TITLE);
    positions_label = make_label(metrics, "", COLOR_NEUTRAL);
    set_secondary_fonts(false);

    /* Amounts, shown only after a tap. They share the lower middle of the screen. */
    value_label = make_label(scr, "", 0xFFFFFF);
    gain_label = make_label(scr, "", COLOR_NEUTRAL);
#if LV_FONT_MONTSERRAT_24
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_24, 0);
#endif
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(gain_label, &lv_font_montserrat_20, 0);
#endif
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, 100);
    lv_obj_align(gain_label, LV_ALIGN_CENTER, 0, 134);
    lv_obj_add_flag(value_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(gain_label, LV_OBJ_FLAG_HIDDEN);

    status_label = make_label(scr, "Waiting for the host...", COLOR_NEUTRAL);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -64);

    /* Runs only while the app is on screen: the framework suspends every app but the first. */
    idle_timer = lv_timer_create(idle_cb, IDLE_POLL_MS, NULL);
}

void equity_ui_suspend(void) {
    if (!idle_timer)
        return;
    lv_timer_pause(idle_timer);
    set_ambient(false, false);
}

void equity_ui_resume(void) {
    if (!idle_timer)
        return;
    set_ambient(false, false);
    lv_timer_resume(idle_timer);
}

static void hide_amounts_cb(lv_timer_t* timer) {
    hide_timer = NULL; /* one-shot: LVGL deletes it after this callback */
    lv_obj_add_flag(value_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(gain_label, LV_OBJ_FLAG_HIDDEN);
}

static void set_status(const equity_summary_t* s) {
    const char* text;
    uint32_t color = COLOR_NEUTRAL;
    char buf[40];

    switch (s->state) {
        case EQUITY_AUTH:
            text = "EquityWatch sign-in failed";
            color = COLOR_LOSS;
            break;
        case EQUITY_OFFLINE:
            text = "EquityWatch offline";
            color = COLOR_WARN;
            break;
        default:
            if (s->age_s < 0) {
                text = s->state == EQUITY_STALE ? "Prices stale" : "";
            } else if (s->age_s < 60) {
                text = "Prices just now";
            } else {
                snprintf(buf, sizeof(buf), "Prices %d min ago", s->age_s / 60);
                text = buf;
            }
            if (s->state == EQUITY_STALE || s->age_s > STALE_AGE_S)
                color = COLOR_WARN;
            break;
    }
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_color(status_label, lv_color_hex(color), 0);
}

void equity_ui_update(const equity_summary_t* s) {
    if (!total_label)
        return;
    set_status(s);

    /* Offline or refused: keep showing the last numbers, only the status line changes. */
    if (s->state == EQUITY_OFFLINE || s->state == EQUITY_AUTH)
        return;

    const bool up = s->total_pct >= 0;
    const uint32_t color = s->total_pct == 0 ? COLOR_NEUTRAL : up ? COLOR_GAIN : COLOR_LOSS;
    char buf[32];

    snprintf(buf, sizeof(buf), "%+.2f%%", (double)s->total_pct);
    lv_label_set_text(total_label, buf);
    lv_label_set_text(arrow_label, s->total_pct == 0 ? "" : up ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(total_label, lv_color_hex(color), 0);
    lv_obj_set_style_text_color(arrow_label, lv_color_hex(color), 0);

    snprintf(buf, sizeof(buf), "Unrealized %+.2f%%", (double)s->unrealized_pct);
    lv_label_set_text(unrealized_label, buf);

    snprintf(buf, sizeof(buf), "%d position%s", s->positions, s->positions == 1 ? "" : "s");
    lv_label_set_text(positions_label, buf);
}

void equity_ui_show_amounts(const char* value, const char* gain, const char* currency) {
    if (!value_label)
        return;
    char buf[48];

    snprintf(buf, sizeof(buf), "Value  %s %s", currency, value);
    lv_label_set_text(value_label, buf);
    snprintf(buf, sizeof(buf), "Gain  %s %s", currency, gain);
    lv_label_set_text(gain_label, buf);
    lv_obj_set_style_text_color(gain_label, lv_color_hex(gain[0] == '-' ? COLOR_LOSS : COLOR_GAIN),
                                0);
    lv_obj_remove_flag(value_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(gain_label, LV_OBJ_FLAG_HIDDEN);

    if (hide_timer)
        lv_timer_delete(hide_timer);
    hide_timer = lv_timer_create(hide_amounts_cb, AMOUNTS_SHOW_MS, NULL);
    lv_timer_set_repeat_count(hide_timer, 1);
}
