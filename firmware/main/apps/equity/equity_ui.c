#include "equity_ui.h"

#include <stdint.h>
#include <stdio.h>

#include "esp_log.h"
#include "status_ui.h"

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
#define GLIDE_PX 24 /* the figures rise this far to make room for the top holdings */
#define ROW_FIRST_Y 70
#define ROW_STEP 30
#define ROW_WIDTH 300
#define COLOR_SYMBOL 0xE0E0E0

/* Going idle runs as a relay rather than all at once, so the board only has one or two things to
 * redraw per frame: the title and status row fade, then the figures rise, then the two lines
 * under the percentage change size, then the top holdings appear. Times are in ms. */
#define FADE_MS 700
#define GLIDE_AT 500
#define GLIDE_MS 800
#define SWAP_AT 1250
#define SWAP_OUT_MS 200
#define SWAP_IN_MS 350
#define ROWS_AT 1750
#define ROWS_MS 600

/* Coming back is quick; the size swap waits until the rest has settled. */
#define BACK_ROWS_MS 200
#define BACK_GLIDE_MS 400
#define BACK_FADE_AT 150
#define BACK_FADE_MS 350
#define BACK_SWAP_AT 450

/* A fade has 255 levels but the eye sees about twenty, and every change is a redraw. */
#define FADE_STEP 12

static lv_obj_t* title_label;
static lv_obj_t* arrow_label;
static lv_obj_t* total_label;
static lv_obj_t* unrealized_label;
static lv_obj_t* positions_label;
static lv_obj_t* status_label;
static lv_timer_t* hide_timer;
static lv_timer_t* idle_timer;
static equity_ui_cmd_cb_t cmd_cb;

static bool ambient;

static bool revealing; /* the AED amounts are replacing the details under the percentage */

/* What the two small lines and the status line say in each mode, and their colours. */
static char text_unrealized[40];
static char text_positions[24];
static char text_status[40] = "Waiting for the host...";
static char text_value[48];
static char text_gain[48];
static uint32_t base_unrealized = COLOR_TITLE;
static uint32_t base_positions = COLOR_NEUTRAL;
static uint32_t base_status = COLOR_NEUTRAL;
static uint32_t color_gain_text = COLOR_GAIN;

/* The four figure lines sit directly on the screen, not in a container: moving or resizing one
 * then redraws only that line, instead of the whole block they share. */
static lv_obj_t* figure_lines[4]; /* arrow, total, unrealized, positions */
static int glide_token;

/* The best performers, listed under the figures while idle. */
static lv_obj_t* top_rows[EQUITY_TOP_COUNT];
static lv_obj_t* top_symbol[EQUITY_TOP_COUNT];
static lv_obj_t* top_pct[EQUITY_TOP_COUNT];
static uint32_t top_pct_color[EQUITY_TOP_COUNT];
static int top_count;
static int32_t rows_fade = 255; /* 0 shown, 255 blended into the background */

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

static int32_t snap(int32_t v) {
    return v >= 255 - FADE_STEP / 2 ? 255 : v / FADE_STEP * FADE_STEP;
}

static void paint_rows(void) {
    for (int i = 0; i < top_count; i++) {
        lv_obj_set_style_text_color(top_symbol[i], mix_bg(COLOR_SYMBOL, rows_fade), 0);
        lv_obj_set_style_text_color(top_pct[i], mix_bg(top_pct_color[i], rows_fade), 0);
    }
}

static void rows_fade_cb(void* var, int32_t v) {
    v = snap(v);
    if (v == rows_fade)
        return;
    rows_fade = v;
    paint_rows();
}

static int32_t title_shade = -1; /* -1 forces the next paint */
static int32_t secondary_shade = -1;

static void title_fade_cb(void* obj, int32_t v) {
    v = snap(v);
    if (v == title_shade)
        return;
    title_shade = v;
    lv_obj_set_style_text_color(obj, mix_bg(COLOR_TITLE, v), 0);
}

static void glide_cb(void* var, int32_t v) {
    static int32_t last = INT32_MIN;
    if (var != NULL && v == last)
        return; /* a whole pixel at a time: most frames of a slow ease change nothing */
    last = v;
    for (int i = 0; i < 4; i++) lv_obj_set_style_translate_y(figure_lines[i], v, 0);
}

static void paint_secondary(int32_t shade) {
    lv_obj_set_style_text_color(unrealized_label, mix_bg(base_unrealized, shade), 0);
    lv_obj_set_style_text_color(positions_label, mix_bg(base_positions, shade), 0);
    lv_obj_set_style_text_color(status_label, mix_bg(base_status, shade), 0);
}

static void secondary_fade_cb(void* obj, int32_t v) {
    v = snap(v);
    if (v == secondary_shade)
        return;
    secondary_shade = v;
    paint_secondary(v);
}

/* Offsets of each line's centre from the screen's centre. */
#define ARROW_Y (-81)
#define TOTAL_Y (-21)
#define UNREALIZED_Y 22
#define POSITIONS_Y 47
#define UNREALIZED_LARGE_Y 25
#define POSITIONS_LARGE_Y 54

static void set_secondary_fonts(bool large) {
    lv_obj_align(unrealized_label, LV_ALIGN_CENTER, 0, large ? UNREALIZED_LARGE_Y : UNREALIZED_Y);
    lv_obj_align(positions_label, LV_ALIGN_CENTER, 0, large ? POSITIONS_LARGE_Y : POSITIONS_Y);
#if LV_FONT_MONTSERRAT_24 && LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(unrealized_label,
                               large ? &lv_font_montserrat_24 : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_font(positions_label,
                               large ? &lv_font_montserrat_20 : &lv_font_montserrat_16, 0);
#endif
}

static void animate(void* obj, lv_anim_exec_xcb_t cb, int32_t from, int32_t to, uint32_t ms,
                    uint32_t delay, lv_anim_path_cb_t path, lv_anim_completed_cb_t done) {
    lv_anim_delete(obj, cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, obj);
    lv_anim_set_exec_cb(&a, cb);
    lv_anim_set_values(&a, from, to);
    lv_anim_set_duration(&a, ms);
    lv_anim_set_delay(&a, delay);
    lv_anim_set_path_cb(&a, path);
    if (done)
        lv_anim_set_completed_cb(&a, done);
    lv_anim_start(&a);
}

/* Put the right words, sizes and colours on the two small lines and the status line for the
 * current mode: the usual details, or the AED amounts after a tap. Call it while they are faded
 * out, or when nothing is animating. */
static void apply_secondary(void) {
    set_secondary_fonts(ambient || revealing);
    if (revealing) {
        lv_label_set_text(unrealized_label, text_value);
        lv_label_set_text(positions_label, text_gain);
        lv_label_set_text(status_label, "");
        base_unrealized = 0xFFFFFF;
        base_positions = color_gain_text;
    } else {
        lv_label_set_text(unrealized_label, text_unrealized);
        lv_label_set_text(positions_label, text_positions);
        lv_label_set_text(status_label, text_status);
        base_unrealized = COLOR_TITLE;
        base_positions = COLOR_NEUTRAL;
    }
}

/* The two lines under the percentage change size: fade out, swap the font, fade back in. */
static void swap_in_cb(lv_anim_t* a) {
    apply_secondary();
    animate(unrealized_label, secondary_fade_cb, 255, 0, SWAP_IN_MS, 0, lv_anim_path_ease_in_out,
            NULL);
}

static void set_ambient(bool on, bool animated) {
    ESP_LOGI(TAG, "ambient %s%s", on ? "on" : "off", animated ? "" : " (reset)");
    ambient = on;
    if (animated && on) {
        animate(title_label, title_fade_cb, 0, 255, FADE_MS, 0, lv_anim_path_ease_in_out, NULL);
        status_ui_set_hidden(true, FADE_MS, 0);
        animate(&glide_token, glide_cb, 0, -GLIDE_PX, GLIDE_MS, GLIDE_AT, lv_anim_path_ease_out,
                NULL);
        animate(unrealized_label, secondary_fade_cb, 0, 255, SWAP_OUT_MS, SWAP_AT,
                lv_anim_path_ease_in_out, swap_in_cb);
        animate(&rows_fade, rows_fade_cb, rows_fade, 0, ROWS_MS, ROWS_AT, lv_anim_path_ease_in_out,
                NULL);
    } else if (animated) {
        animate(&rows_fade, rows_fade_cb, rows_fade, 255, BACK_ROWS_MS, 0, lv_anim_path_ease_in_out,
                NULL);
        animate(&glide_token, glide_cb, -GLIDE_PX, 0, BACK_GLIDE_MS, 0, lv_anim_path_ease_out,
                NULL);
        animate(title_label, title_fade_cb, 255, 0, BACK_FADE_MS, BACK_FADE_AT,
                lv_anim_path_ease_in_out, NULL);
        status_ui_set_hidden(false, BACK_FADE_MS, BACK_FADE_AT);
        animate(unrealized_label, secondary_fade_cb, 0, 255, SWAP_OUT_MS, BACK_SWAP_AT,
                lv_anim_path_ease_in_out, swap_in_cb);
    } else {
        lv_anim_delete(title_label, title_fade_cb);
        lv_anim_delete(&glide_token, glide_cb);
        lv_anim_delete(unrealized_label, secondary_fade_cb);
        lv_anim_delete(&rows_fade, rows_fade_cb);
        title_shade = secondary_shade = -1;
        title_fade_cb(title_label, on ? 255 : 0);
        glide_cb(NULL, on ? -GLIDE_PX : 0);
        secondary_fade_cb(NULL, 0);
        apply_secondary();
        rows_fade = -1;
        rows_fade_cb(NULL, on ? 0 : 255);
        status_ui_set_hidden(on, 0, 0);
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

    arrow_label = make_label(scr, "", COLOR_NEUTRAL);
    total_label = make_label(scr, "---", COLOR_NEUTRAL);
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(arrow_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_font(total_label, &lv_font_montserrat_48, 0);
#endif
    lv_obj_align(arrow_label, LV_ALIGN_CENTER, 0, ARROW_Y);
    lv_obj_align(total_label, LV_ALIGN_CENTER, 0, TOTAL_Y);
    unrealized_label = make_label(scr, "", COLOR_TITLE);
    positions_label = make_label(scr, "", COLOR_NEUTRAL);
    set_secondary_fonts(false);
    figure_lines[0] = arrow_label;
    figure_lines[1] = total_label;
    figure_lines[2] = unrealized_label;
    figure_lines[3] = positions_label;

    /* The three best performers, shown only once the screen has gone idle. */
    for (int i = 0; i < EQUITY_TOP_COUNT; i++) {
        top_rows[i] = lv_obj_create(scr);
        lv_obj_remove_style_all(top_rows[i]);
        lv_obj_remove_flag(top_rows[i], LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_size(top_rows[i], ROW_WIDTH, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(top_rows[i], LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(top_rows[i], LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_align(top_rows[i], LV_ALIGN_CENTER, 0, ROW_FIRST_Y + ROW_STEP * i);
        top_symbol[i] = make_label(top_rows[i], "", COLOR_SYMBOL);
        top_pct[i] = make_label(top_rows[i], "", COLOR_NEUTRAL);
#if LV_FONT_MONTSERRAT_20
        lv_obj_set_style_text_font(top_symbol[i], &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_font(top_pct[i], &lv_font_montserrat_20, 0);
#endif
        lv_obj_add_flag(top_rows[i], LV_OBJ_FLAG_HIDDEN);
    }

    status_label = make_label(scr, "Waiting for the host...", COLOR_NEUTRAL);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -64);

    /* Runs only while the app is on screen: the framework suspends every app but the first. */
    idle_timer = lv_timer_create(idle_cb, IDLE_POLL_MS, NULL);
}

/* Leaving the screen ends a reveal and any idle effect, so it is back to normal on return. */
static void reset_view(void) {
    if (hide_timer) {
        lv_timer_delete(hide_timer);
        hide_timer = NULL;
    }
    revealing = false;
    set_ambient(false, false);
}

void equity_ui_suspend(void) {
    if (!idle_timer)
        return;
    lv_timer_pause(idle_timer);
    reset_view();
}

void equity_ui_resume(void) {
    if (!idle_timer)
        return;
    reset_view();
    lv_timer_resume(idle_timer);
}

static void dip_secondary(uint32_t delay) {
    animate(unrealized_label, secondary_fade_cb, secondary_shade < 0 ? 0 : secondary_shade, 255,
            SWAP_OUT_MS, delay, lv_anim_path_ease_in_out, swap_in_cb);
}

static void hide_amounts_cb(lv_timer_t* timer) {
    hide_timer = NULL; /* one-shot: LVGL deletes it after this callback */
    revealing = false;
    dip_secondary(0);
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
    snprintf(text_status, sizeof(text_status), "%s", text);
    base_status = color;
    if (!revealing) { /* during a reveal the status line stays blank */
        lv_label_set_text(status_label, text_status);
        paint_secondary(secondary_shade < 0 ? 0 : secondary_shade);
    }
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

    snprintf(text_unrealized, sizeof(text_unrealized), "Unrealized %+.2f%%",
             (double)s->unrealized_pct);
    snprintf(text_positions, sizeof(text_positions), "%d position%s", s->positions,
             s->positions == 1 ? "" : "s");
    if (!revealing) {
        lv_label_set_text(unrealized_label, text_unrealized);
        lv_label_set_text(positions_label, text_positions);
    }
}

void equity_ui_set_top(const equity_top_t* top) {
    if (!top_rows[0])
        return;
    top_count = top->count;
    for (int i = 0; i < EQUITY_TOP_COUNT; i++) {
        if (i >= top_count) {
            lv_obj_add_flag(top_rows[i], LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        char buf[16];
        snprintf(buf, sizeof(buf), "%+.1f%%", (double)top->pct[i]);
        lv_label_set_text(top_symbol[i], top->symbol[i]);
        lv_label_set_text(top_pct[i], buf);
        top_pct_color[i] = top->pct[i] == 0  ? COLOR_NEUTRAL
                           : top->pct[i] > 0 ? COLOR_GAIN
                                             : COLOR_LOSS;
        lv_obj_remove_flag(top_rows[i], LV_OBJ_FLAG_HIDDEN);
    }
    paint_rows();
}

void equity_ui_show_amounts(const char* value, const char* gain, const char* currency) {
    if (!unrealized_label)
        return;
    snprintf(text_value, sizeof(text_value), "%s %s", currency, value);
    snprintf(text_gain, sizeof(text_gain), "Gain  %s", gain);
    color_gain_text = gain[0] == '-' ? COLOR_LOSS : COLOR_GAIN;

    /* The amounts take the place of the details: fade those out, swap the words, fade back in. */
    revealing = true;
    dip_secondary(0);

    if (hide_timer)
        lv_timer_delete(hide_timer);
    hide_timer = lv_timer_create(hide_amounts_cb, AMOUNTS_SHOW_MS, NULL);
    lv_timer_set_repeat_count(hide_timer, 1);
}
