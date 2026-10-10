#include "spotify_ui.h"

#include <stdio.h>

static lv_obj_t* spotify_track_label;
static lv_obj_t* spotify_artist_label;
static lv_obj_t* spotify_progress_bar;
static lv_obj_t* spotify_time_label;
static lv_obj_t* btn_play_pause_label;
static lv_obj_t* spotify_art_note;
static lv_obj_t* spotify_art_img;
static lv_obj_t* spotify_art_container;
static lv_obj_t* spotify_header;
static lv_obj_t* spotify_ctrl_btns[3];
static lv_image_dsc_t spotify_art_dsc;
static spotify_ui_cmd_cb_t s_cmd_cb = NULL;

#define SPOTIFY_ART_SIZE 180

static void ctrl_btn_cb(lv_event_t* e) {
    if (s_cmd_cb)
        s_cmd_cb(lv_event_get_user_data(e));
}

static lv_obj_t* make_ctrl_btn(lv_obj_t* parent, const char* symbol, const char* cmd,
                               int x_offset) {
    lv_obj_t* btn = lv_button_create(parent);
    lv_obj_set_size(btn, 75, 75);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x282828), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1DB954), LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(btn, 0, 0);
    lv_obj_align(btn, LV_ALIGN_CENTER, x_offset, 150);
    lv_obj_add_event_cb(btn, ctrl_btn_cb, LV_EVENT_CLICKED, (void*)cmd);

    lv_obj_t* lbl = lv_label_create(btn);
    lv_label_set_text(lbl, symbol);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(lbl);

    return lbl;
}

void spotify_ui_create(lv_obj_t* scr, spotify_ui_cmd_cb_t on_cmd) {
    s_cmd_cb = on_cmd;

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);

    spotify_header = lv_label_create(scr);
    lv_label_set_text(spotify_header, "Now Playing");
    lv_obj_set_style_text_color(spotify_header, lv_color_hex(0x1DB954), 0);
    lv_obj_align(spotify_header, LV_ALIGN_TOP_MID, 0, 24);

    spotify_art_container = lv_obj_create(scr);
    lv_obj_t* art = spotify_art_container;
    lv_obj_set_size(art, SPOTIFY_ART_SIZE, SPOTIFY_ART_SIZE);
    lv_obj_set_style_bg_color(art, lv_color_hex(0x282828), 0);
    lv_obj_set_style_border_width(art, 0, 0);
    lv_obj_set_style_radius(art, 12, 0);
    lv_obj_set_scrollbar_mode(art, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(art, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(art, LV_ALIGN_TOP_MID, 0, 55);

    spotify_art_note = lv_label_create(art);
    lv_label_set_text(spotify_art_note, LV_SYMBOL_AUDIO);
    lv_obj_set_style_text_color(spotify_art_note, lv_color_hex(0x1DB954), 0);
    lv_obj_set_style_text_font(spotify_art_note, &lv_font_montserrat_48, 0);
    lv_obj_center(spotify_art_note);

    spotify_art_img = lv_image_create(art);
    lv_obj_set_size(spotify_art_img, SPOTIFY_ART_SIZE, SPOTIFY_ART_SIZE);
    lv_obj_set_style_radius(spotify_art_img, 12, 0);
    lv_obj_set_style_clip_corner(spotify_art_img, true, 0);
    lv_obj_set_scrollbar_mode(spotify_art_img, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(spotify_art_img, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(spotify_art_img);
    lv_obj_add_flag(spotify_art_img, LV_OBJ_FLAG_HIDDEN);

    spotify_track_label = lv_label_create(scr);
    lv_label_set_text(spotify_track_label, "---");
    lv_label_set_long_mode(spotify_track_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(spotify_track_label, 340);
    lv_obj_set_style_text_color(spotify_track_label, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(spotify_track_label, &lv_font_montserrat_20, 0);
#endif
    lv_obj_align(spotify_track_label, LV_ALIGN_TOP_MID, 0, 246);

    spotify_artist_label = lv_label_create(scr);
    lv_label_set_text(spotify_artist_label, "---");
    lv_label_set_long_mode(spotify_artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(spotify_artist_label, 340);
    lv_obj_set_style_text_color(spotify_artist_label, lv_color_hex(0xB3B3B3), 0);
    lv_obj_align(spotify_artist_label, LV_ALIGN_TOP_MID, 0, 278);

    spotify_progress_bar = lv_bar_create(scr);
    lv_obj_set_size(spotify_progress_bar, 300, 6);
    lv_bar_set_range(spotify_progress_bar, 0, 1000);
    lv_bar_set_value(spotify_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(spotify_progress_bar, lv_color_hex(0x535353), LV_PART_MAIN);
    lv_obj_set_style_bg_color(spotify_progress_bar, lv_color_hex(0x1DB954), LV_PART_INDICATOR);
    lv_obj_set_style_radius(spotify_progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(spotify_progress_bar, 3, LV_PART_INDICATOR);
    lv_obj_align(spotify_progress_bar, LV_ALIGN_TOP_MID, 0, 310);

    spotify_time_label = lv_label_create(scr);
    lv_label_set_text(spotify_time_label, "0:00 / 0:00");
    lv_obj_set_style_text_color(spotify_time_label, lv_color_hex(0x535353), 0);
    lv_obj_align(spotify_time_label, LV_ALIGN_TOP_MID, 0, 322);

    spotify_ctrl_btns[0] = lv_obj_get_parent(make_ctrl_btn(scr, LV_SYMBOL_PREV, "prev", -95));
    btn_play_pause_label = make_ctrl_btn(scr, LV_SYMBOL_PLAY, "play_pause", 0);
    spotify_ctrl_btns[1] = lv_obj_get_parent(btn_play_pause_label);
    spotify_ctrl_btns[2] = lv_obj_get_parent(make_ctrl_btn(scr, LV_SYMBOL_NEXT, "next", 95));
}

static void ms_to_str(int32_t ms, char* buf, size_t len) {
    int32_t total_sec = ms / 1000;
    snprintf(buf, len, "%ld:%02ld", (long)(total_sec / 60), (long)(total_sec % 60));
}

void spotify_ui_update(const spotify_track_t* t) {
    if (!spotify_track_label || !spotify_artist_label)
        return;

    const bool is_playing = t->is_playing;
    const int32_t progress_ms = t->progress_ms;
    const int32_t duration_ms = t->duration_ms;

    lv_label_set_text(spotify_track_label, t->track);
    lv_label_set_text(spotify_artist_label, t->artist);

    if (btn_play_pause_label)
        lv_label_set_text(btn_play_pause_label, is_playing ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

    if (spotify_progress_bar && duration_ms > 0) {
        int32_t pct = (int32_t)((progress_ms * 1000LL) / duration_ms);
        lv_bar_set_value(spotify_progress_bar, pct, LV_ANIM_ON);
    }

    if (spotify_time_label) {
        char cur[16], tot[16], buf[40];
        ms_to_str(progress_ms, cur, sizeof(cur));
        ms_to_str(duration_ms, tot, sizeof(tot));
        snprintf(buf, sizeof(buf), "%s / %s", cur, tot);
        lv_label_set_text(spotify_time_label, buf);
    }
}

void spotify_ui_set_art(const uint8_t* data, int w, int h, uint32_t color) {
    if (!spotify_art_img || !data || w <= 0 || h <= 0)
        return;

    spotify_art_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    spotify_art_dsc.header.cf = LV_COLOR_FORMAT_RGB565;
    spotify_art_dsc.header.w = (uint32_t)w;
    spotify_art_dsc.header.h = (uint32_t)h;
    spotify_art_dsc.header.stride = (uint32_t)(w * 2);
    spotify_art_dsc.data_size = (uint32_t)(w * h * 2);
    spotify_art_dsc.data = data;

    lv_image_set_src(spotify_art_img, &spotify_art_dsc);

    if (spotify_art_note)
        lv_obj_add_flag(spotify_art_note, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(spotify_art_img, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(spotify_art_img);

    lv_color_t accent = lv_color_hex(color);

    if (spotify_header)
        lv_obj_set_style_text_color(spotify_header, accent, 0);

    if (spotify_progress_bar)
        lv_obj_set_style_bg_color(spotify_progress_bar, accent, LV_PART_INDICATOR);

    for (int i = 0; i < 3; i++)
        if (spotify_ctrl_btns[i])
            lv_obj_set_style_bg_color(spotify_ctrl_btns[i], accent, LV_STATE_PRESSED);

    if (spotify_art_container) {
        lv_obj_set_style_shadow_color(spotify_art_container, accent, 0);
        lv_obj_set_style_shadow_width(spotify_art_container, 30, 0);
        lv_obj_set_style_shadow_spread(spotify_art_container, 2, 0);
        lv_obj_set_style_shadow_opa(spotify_art_container, LV_OPA_60, 0);
    }
}
