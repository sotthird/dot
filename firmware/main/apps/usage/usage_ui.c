#include "usage_ui.h"

#include <stdio.h>
#include <stdlib.h>

#define COL_BG 0x121212 /* the status row fades into this */
#define COL_PANEL 0x1f1f1e
#define COL_TRACK 0x2a2a28
#define COL_TEXT 0xfaf9f5
#define COL_DIM 0xb0aea5
#define COL_GREEN 0x788c5d
#define COL_AMBER 0xd97757
#define COL_RED 0xc0392b

#define PANEL_X 40
#define PANEL_W 400
#define PANEL_H 165
#define PANEL_PAD 18
#define BAR_H 24

typedef struct {
    lv_obj_t* pct;
    lv_obj_t* bar;
    lv_obj_t* caption;
} window_t;

static lv_obj_t* summary_label;
static window_t five_hours_win;
static window_t week_win;

static lv_obj_t* make_label(lv_obj_t* parent, uint32_t color, const lv_font_t* font) {
    lv_obj_t* label = lv_label_create(parent);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(label, font, 0);
    return label;
}

/* A card in the style of Clawdmeter: the figure at top left, a pill naming the window at top
 * right, a rounded bar, and a dim line of detail beneath. */
static void make_window(lv_obj_t* scr, window_t* win, const char* pill_text, int y) {
    lv_obj_t* panel = lv_obj_create(scr);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(panel, PANEL_X, y);
    lv_obj_set_size(panel, PANEL_W, PANEL_H);
    lv_obj_set_style_bg_color(panel, lv_color_hex(COL_PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(panel, 0, 0);
    lv_obj_set_style_radius(panel, 16, 0);
    lv_obj_set_style_pad_all(panel, PANEL_PAD, 0);

    win->pct = make_label(panel, COL_TEXT, &lv_font_montserrat_48);
    lv_label_set_text(win->pct, "--");
    lv_obj_set_pos(win->pct, 0, 0);

    lv_obj_t* pill = make_label(panel, COL_TEXT, &lv_font_montserrat_20);
    lv_label_set_text(pill, pill_text);
    lv_obj_set_style_bg_color(pill, lv_color_hex(COL_TRACK), 0);
    lv_obj_set_style_bg_opa(pill, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(pill, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_pad_hor(pill, 16, 0);
    lv_obj_set_style_pad_ver(pill, 6, 0);
    lv_obj_align(pill, LV_ALIGN_TOP_RIGHT, 0, 4);

    win->bar = lv_bar_create(panel);
    lv_obj_set_pos(win->bar, 0, 66);
    lv_obj_set_size(win->bar, PANEL_W - 2 * PANEL_PAD, BAR_H);
    lv_bar_set_range(win->bar, 0, 100);
    lv_bar_set_value(win->bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(win->bar, lv_color_hex(COL_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(win->bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(win->bar, 6, LV_PART_MAIN);
    lv_obj_set_style_bg_color(win->bar, lv_color_hex(COL_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(win->bar, LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_set_style_radius(win->bar, 6, LV_PART_INDICATOR);

    win->caption = make_label(panel, COL_DIM, &lv_font_montserrat_20);
    lv_label_set_text(win->caption, "");
    lv_obj_set_pos(win->caption, 0, 100);
}

void usage_ui_create(lv_obj_t* scr) {
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);

    summary_label = make_label(scr, COL_DIM, &lv_font_montserrat_20);
    lv_label_set_text(summary_label, "Claude tokens today");
    lv_obj_align(summary_label, LV_ALIGN_TOP_MID, 0, 28);

    make_window(scr, &five_hours_win, "5 hours", 68);
    make_window(scr, &week_win, "7 days", 68 + PANEL_H + 12);
}

static uint32_t level_color(int pct) {
    if (pct >= 80)
        return COL_RED;
    if (pct >= 50)
        return COL_AMBER;
    return COL_GREEN;
}

/* Show a window as a percentage of its budget over a bar, with the token count beneath. With no
 * budget (a negative percentage) the token count takes the place of the percentage. */
static void set_window(window_t* win, const char* tokens, const char* pct_text,
                       const char* reset_text) {
    char buf[32];
    int pct = atoi(pct_text);
    int reset = atoi(reset_text);

    if (pct < 0) {
        lv_label_set_text(win->pct, tokens);
        lv_label_set_text(win->caption, "no usage data");
        lv_bar_set_value(win->bar, 0, LV_ANIM_ON);
        return;
    }
    snprintf(buf, sizeof(buf), "%d%%", pct);
    lv_label_set_text(win->pct, buf);
    if (reset < 0)
        snprintf(buf, sizeof(buf), "%s tokens", tokens);
    else if (reset < 60)
        snprintf(buf, sizeof(buf), "Resets in %dm", reset);
    else if (reset < 1440)
        snprintf(buf, sizeof(buf), "Resets in %dh %dm", reset / 60, reset % 60);
    else
        snprintf(buf, sizeof(buf), "Resets in %dd %dh", reset / 1440, (reset % 1440) / 60);
    lv_label_set_text(win->caption, buf);
    lv_bar_set_value(win->bar, pct > 100 ? 100 : pct, LV_ANIM_ON);
    lv_obj_set_style_bg_color(win->bar, lv_color_hex(level_color(pct)), LV_PART_INDICATOR);
}

void usage_ui_update(const char* today, const char* five_hours, const char* week,
                     const char* messages, const char* pct_5h, const char* pct_7d,
                     const char* reset_5h, const char* reset_7d) {
    if (!summary_label)
        return;

    char buf[48];
    if (today[0] == '-') /* standalone: no transcripts, so no token counts */
        snprintf(buf, sizeof(buf), "Claude plan usage");
    else
        snprintf(buf, sizeof(buf), "Today: %s tokens, %s msgs", today, messages);
    lv_label_set_text(summary_label, buf);

    set_window(&five_hours_win, five_hours, pct_5h, reset_5h);
    set_window(&week_win, week, pct_7d, reset_7d);
}
