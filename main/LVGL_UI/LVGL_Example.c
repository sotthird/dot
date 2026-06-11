#include "LVGL_Example.h"

#include <stdio.h>

static lv_obj_t* cpu_arc;
static lv_obj_t* cpu_pct_label;

void Lvgl_Example1(void) {
    lv_obj_t* scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x1a1a2e), 0);

    lv_obj_t* title = lv_label_create(scr);
    lv_label_set_text(title, "Laptop CPU Usage");
    lv_obj_set_style_text_color(title, lv_color_hex(0xe0e0e0), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 30);

    cpu_arc = lv_arc_create(scr);
    lv_obj_set_size(cpu_arc, 300, 300);
    lv_arc_set_rotation(cpu_arc, 135);
    lv_arc_set_bg_angles(cpu_arc, 0, 270);
    lv_arc_set_range(cpu_arc, 0, 100);
    lv_arc_set_value(cpu_arc, 0);
    lv_obj_remove_flag(cpu_arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(cpu_arc, lv_color_hex(0x16213e), LV_PART_MAIN);
    lv_obj_set_style_arc_color(cpu_arc, lv_color_hex(0x0f3460), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(cpu_arc, 20, LV_PART_MAIN);
    lv_obj_set_style_arc_width(cpu_arc, 20, LV_PART_INDICATOR);
    lv_obj_center(cpu_arc);

    cpu_pct_label = lv_label_create(scr);
    lv_label_set_text(cpu_pct_label, "---%");
    lv_obj_set_style_text_color(cpu_pct_label, lv_color_hex(0xe94560), 0);
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(cpu_pct_label, &lv_font_montserrat_48, 0);
#endif
    lv_obj_center(cpu_pct_label);
}

void update_cpu_display(float cpu_pct) {
    if (!cpu_pct_label || !cpu_arc)
        return;
    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f%%", cpu_pct);
    lv_label_set_text(cpu_pct_label, buf);
    lv_arc_set_value(cpu_arc, (int32_t)cpu_pct);

    uint8_t r = (uint8_t)(cpu_pct * 2.55f);
    uint8_t g = (uint8_t)((100.0f - cpu_pct) * 1.5f);
    lv_obj_set_style_arc_color(cpu_arc, lv_color_make(r, g, 30), LV_PART_INDICATOR);
    lv_obj_set_style_text_color(cpu_pct_label, lv_color_make(r, g, 30), 0);
}

static lv_obj_t* spotify_track_label;
static lv_obj_t* spotify_artist_label;
static lv_obj_t* spotify_progress_bar;
static lv_obj_t* spotify_time_label;
static lv_obj_t* btn_play_pause_label;
static spotify_cmd_cb_t s_cmd_cb = NULL;

void spotify_set_cmd_callback(spotify_cmd_cb_t cb) {
    s_cmd_cb = cb;
}

static void btn_prev_cb(lv_event_t* e) {
    (void)e;
    if (s_cmd_cb)
        s_cmd_cb("prev");
}

static void btn_play_pause_cb(lv_event_t* e) {
    (void)e;
    if (s_cmd_cb)
        s_cmd_cb("play_pause");
}

static void btn_next_cb(lv_event_t* e) {
    (void)e;
    if (s_cmd_cb)
        s_cmd_cb("next");
}

static lv_obj_t* make_ctrl_btn(lv_obj_t* parent, const char* symbol, lv_event_cb_t cb,
                               int x_offset) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 75, 75);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x282828), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1DB954), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_align(btn, LV_ALIGN_CENTER, x_offset, 150);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);

    return lbl;
}

void spotify(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_screen_load(scr);

    lv_obj_t* header = lv_label_create(scr);
    lv_label_set_text(header, "Now Playing");
    lv_obj_set_style_text_color(header, lv_color_hex(0x1DB954), 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t* art = lv_obj_create(scr);
    lv_obj_set_size(art, 170, 170);
    lv_obj_set_style_bg_color(art, lv_color_hex(0x282828), 0);
    lv_obj_set_style_border_width(art, 0, 0);
    lv_obj_set_style_radius(art, 12, 0);
    lv_obj_align(art, LV_ALIGN_TOP_MID, 0, 55);

    lv_obj_t* note = lv_label_create(art);
    lv_label_set_text(note, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(note, lv_color_hex(0x1DB954), 0);
    lv_obj_set_style_text_font(note, &lv_font_montserrat_48, 0);
    lv_obj_center(note);

    spotify_track_label = lv_label_create(scr);
    lv_label_set_text(spotify_track_label, "---");
    lv_label_set_long_mode(spotify_track_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(spotify_track_label, 340);
    lv_obj_set_style_text_color(spotify_track_label, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(spotify_track_label, &lv_font_montserrat_20, 0);
#endif
    lv_obj_align(spotify_track_label, LV_ALIGN_TOP_MID, 0, 238);

    spotify_artist_label = lv_label_create(scr);
    lv_label_set_text(spotify_artist_label, "---");
    lv_label_set_long_mode(spotify_artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(spotify_artist_label, 340);
    lv_obj_set_style_text_color(spotify_artist_label, lv_color_hex(0xB3B3B3), 0);
    lv_obj_align(spotify_artist_label, LV_ALIGN_TOP_MID, 0, 268);

    spotify_progress_bar = lv_bar_create(scr);
    lv_obj_set_size(spotify_progress_bar, 300, 6);
    lv_bar_set_range(spotify_progress_bar, 0, 1000);
    lv_bar_set_value(spotify_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(spotify_progress_bar, lv_color_hex(0x535353), LV_PART_MAIN);
    lv_obj_set_style_bg_color(spotify_progress_bar, lv_color_hex(0x1DB954), LV_PART_INDICATOR);
    lv_obj_set_style_radius(spotify_progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(spotify_progress_bar, 3, LV_PART_INDICATOR);
    lv_obj_align(spotify_progress_bar, LV_ALIGN_TOP_MID, 0, 300);

    spotify_time_label = lv_label_create(scr);
    lv_label_set_text(spotify_time_label, "0:00 / 0:00");
    lv_obj_set_style_text_color(spotify_time_label, lv_color_hex(0x535353), 0);
    lv_obj_align(spotify_time_label, LV_ALIGN_TOP_MID, 0, 315);

    make_ctrl_btn(scr, LV_SYMBOL_PREV, btn_prev_cb, -95);
    btn_play_pause_label = make_ctrl_btn(scr, LV_SYMBOL_PLAY, btn_play_pause_cb, 0);
    make_ctrl_btn(scr, LV_SYMBOL_NEXT, btn_next_cb, 95);
}

static void ms_to_str(int32_t ms, char* buf, size_t len) {
    int32_t total_sec = ms / 1000;
    snprintf(buf, len, "%ld:%02ld", (long)(total_sec / 60), (long)(total_sec % 60));
}

void update_spotify(const char* track, const char* artist, bool is_playing, int32_t progress_ms,
                    int32_t duration_ms) {
    if (!spotify_track_label || !spotify_artist_label)
        return;

    lv_label_set_text(spotify_track_label, track);
    lv_label_set_text(spotify_artist_label, artist);

    if (btn_play_pause_label)
        lv_label_set_text(btn_play_pause_label, is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    if (spotify_progress_bar && duration_ms > 0) {
        int32_t pct = (int32_t)((progress_ms * 1000LL) / duration_ms);
        lv_bar_set_value(spotify_progress_bar, pct, LV_ANIM_ON);
    }

    if (spotify_time_label) {
        char cur[8], tot[8], buf[20];
        ms_to_str(progress_ms, cur, sizeof(cur));
        ms_to_str(duration_ms, tot, sizeof(tot));
        snprintf(buf, sizeof(buf), "%s / %s", cur, tot);
        lv_label_set_text(spotify_time_label, buf);
    }
}
