#include "LVGL_Example.h"

#include <stdio.h>
#include <string.h>

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
static lv_obj_t* spotify_art_note;
static lv_obj_t* spotify_art_img;
static lv_obj_t* spotify_art_container;
static lv_obj_t* spotify_header;
static lv_obj_t* spotify_ctrl_btns[3];
static lv_image_dsc_t spotify_art_dsc;
static spotify_cmd_cb_t s_cmd_cb = NULL;

#define SPOTIFY_ART_SIZE 180

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
    lv_obj_align(spotify_track_label, LV_ALIGN_TOP_MID, 0, 252);

    spotify_artist_label = lv_label_create(scr);
    lv_label_set_text(spotify_artist_label, "---");
    lv_label_set_long_mode(spotify_artist_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(spotify_artist_label, 340);
    lv_obj_set_style_text_color(spotify_artist_label, lv_color_hex(0xB3B3B3), 0);
    lv_obj_align(spotify_artist_label, LV_ALIGN_TOP_MID, 0, 274);

    spotify_progress_bar = lv_bar_create(scr);
    lv_obj_set_size(spotify_progress_bar, 300, 6);
    lv_bar_set_range(spotify_progress_bar, 0, 1000);
    lv_bar_set_value(spotify_progress_bar, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(spotify_progress_bar, lv_color_hex(0x535353), LV_PART_MAIN);
    lv_obj_set_style_bg_color(spotify_progress_bar, lv_color_hex(0x1DB954), LV_PART_INDICATOR);
    lv_obj_set_style_radius(spotify_progress_bar, 3, LV_PART_MAIN);
    lv_obj_set_style_radius(spotify_progress_bar, 3, LV_PART_INDICATOR);
    lv_obj_align(spotify_progress_bar, LV_ALIGN_TOP_MID, 0, 304);

    spotify_time_label = lv_label_create(scr);
    lv_label_set_text(spotify_time_label, "0:00 / 0:00");
    lv_obj_set_style_text_color(spotify_time_label, lv_color_hex(0x535353), 0);
    lv_obj_align(spotify_time_label, LV_ALIGN_TOP_MID, 0, 319);

    spotify_ctrl_btns[0] = lv_obj_get_parent(make_ctrl_btn(scr, LV_SYMBOL_PREV, btn_prev_cb, -95));
    btn_play_pause_label = make_ctrl_btn(scr, LV_SYMBOL_PLAY, btn_play_pause_cb, 0);
    spotify_ctrl_btns[1] = lv_obj_get_parent(btn_play_pause_label);
    spotify_ctrl_btns[2] = lv_obj_get_parent(make_ctrl_btn(scr, LV_SYMBOL_NEXT, btn_next_cb, 95));
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

void update_spotify_art(const uint8_t* data, int w, int h, uint32_t color) {
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

/* ---- CI / build status orb ------------------------------------------------ */

#define CI_CENTER_SIZE 210
#define CI_RING_SIZE 250
#define CI_RING_WIDTH 16
#define CI_ORB_Y (-20)

/* Per-state palette: gradient base -> darker (center fill), ring rim, rim highlight (pulse). */
typedef struct {
    uint32_t base;
    uint32_t dark;
    uint32_t rim;
    uint32_t rim_bright;
} ci_palette_t;

/* Indexed by state: 0 unknown, 1 running, 2 success, 3 failure. */
static const ci_palette_t ci_pal[4] = {
    {0x3A3A3A, 0x222222, 0x555555, 0x777777},  // unknown
    {0xF5A623, 0xB37714, 0xFFC15A, 0xFFD98C},  // running
    {0x2ECC71, 0x1E8E50, 0x57E08A, 0x8DF0B5},  // success
    {0xE74C3C, 0xA93226, 0xF1786B, 0xF7A79E},  // failure
};

static lv_obj_t* ci_ring;
static lv_obj_t* ci_center;
static lv_obj_t* ci_icon_label;
static lv_obj_t* ci_word_label;
static lv_obj_t* ci_sub_label;
static lv_obj_t* ci_repo_label;
static lv_obj_t* ci_title_label;
static lv_obj_t* ci_wf_label;
static ci_cmd_cb_t ci_cmd_cb = NULL;

static lv_timer_t* ci_dots_timer = NULL;
static uint32_t ci_pulse_base = 0x555555;
static uint32_t ci_pulse_bright = 0x777777;

/* Last rendered status, kept so a refresh can revert if no fresh data arrives. */
static lv_timer_t* ci_refresh_timer = NULL;
static int ci_last_state = 0;
static char ci_last_repo[64] = "";
static char ci_last_title[96] = "";
static char ci_last_wf[64] = "";
static char ci_last_info[32] = "";

#define CI_REFRESH_BASE 0x3A6EA5
#define CI_REFRESH_BRIGHT 0x7FB3E8
#define CI_REFRESH_DARK 0x244A6E

static void ci_show_refreshing(void);

void ci_set_cmd_callback(ci_cmd_cb_t cb) {
    ci_cmd_cb = cb;
}

static void ci_orb_clicked_cb(lv_event_t* e) {
    (void)e;
    ci_show_refreshing();
    if (ci_cmd_cb)
        ci_cmd_cb("ci_refresh");
}

/* Software-rendered panel, no GPU: keep continuous work tiny. While running we
 * only (a) re-fill the thin rim ring with a pulsing color (solid fill, no blend)
 * and (b) rewrite a few dot characters. Everything else is static. */

static void ci_dots_cb(lv_timer_t* t) {
    (void)t;
    static const char* frames[] = {".", "..", "..."};
    static int i = 0;
    if (ci_icon_label)
        lv_label_set_text(ci_icon_label, frames[i++ % 3]);
}

static void ci_start_dots(void) {
    if (!ci_icon_label)
        return;
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(ci_icon_label, &lv_font_montserrat_20, 0);
#endif
    lv_label_set_text(ci_icon_label, ".");
    if (!ci_dots_timer)
        ci_dots_timer = lv_timer_create(ci_dots_cb, 350, NULL);
}

static void ci_stop_dots(void) {
    if (ci_dots_timer) {
        lv_timer_delete(ci_dots_timer);
        ci_dots_timer = NULL;
    }
#if LV_FONT_MONTSERRAT_48
    if (ci_icon_label)
        lv_obj_set_style_text_font(ci_icon_label, &lv_font_montserrat_48, 0);
#endif
}

static void ci_set_ring_mix(void* obj, int32_t ratio) {
    lv_color_t c =
        lv_color_mix(lv_color_hex(ci_pulse_bright), lv_color_hex(ci_pulse_base), (uint8_t)ratio);
    lv_obj_set_style_arc_color((lv_obj_t*)obj, c, LV_PART_INDICATOR);
}

static void ci_start_ring_pulse(void) {
    if (!ci_ring)
        return;
    lv_anim_delete(ci_ring, ci_set_ring_mix);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, ci_ring);
    lv_anim_set_exec_cb(&a, ci_set_ring_mix);
    lv_anim_set_values(&a, 0, 255);
    lv_anim_set_duration(&a, 1200);
    lv_anim_set_reverse_duration(&a, 1200);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

static void ci_stop_ring_pulse(void) {
    if (!ci_ring)
        return;
    lv_anim_delete(ci_ring, ci_set_ring_mix);
}

/* If a refresh tap gets no fresh data in time (e.g. host offline), revert. */
static void ci_refresh_timeout_cb(lv_timer_t* t) {
    (void)t;
    ci_refresh_timer = NULL;  // one-shot: LVGL deletes it after this callback
    update_ci_orb(ci_last_state, ci_last_repo, ci_last_title, ci_last_wf, ci_last_info);
}

/* Immediate on-tap feedback: blue pulsing ring + animated dots while we wait for
 * the host to re-fetch. The next update_ci_orb() cancels this and shows the result. */
static void ci_show_refreshing(void) {
    if (ci_word_label)
        lv_label_set_text(ci_word_label, "REFRESH");
    if (ci_sub_label)
        lv_label_set_text(ci_sub_label, "");

    /* Turn the whole orb blue while refreshing. */
    lv_obj_set_style_bg_color(ci_center, lv_color_hex(CI_REFRESH_BASE), 0);
    lv_obj_set_style_border_color(ci_center, lv_color_hex(CI_REFRESH_DARK), 0);

    ci_pulse_base = CI_REFRESH_BASE;
    ci_pulse_bright = CI_REFRESH_BRIGHT;
    if (ci_ring)
        lv_obj_set_style_arc_color(ci_ring, lv_color_hex(CI_REFRESH_BASE), LV_PART_INDICATOR);
    ci_start_dots();
    ci_start_ring_pulse();

    if (ci_refresh_timer)
        lv_timer_delete(ci_refresh_timer);
    ci_refresh_timer = lv_timer_create(ci_refresh_timeout_cb, 6000, NULL);
    lv_timer_set_repeat_count(ci_refresh_timer, 1);
}

void ci_orb(void) {
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_screen_load(scr);

    ci_repo_label = lv_label_create(scr);
    lv_label_set_text(ci_repo_label, "CI Status");
    lv_obj_set_style_text_color(ci_repo_label, lv_color_hex(0xB3B3B3), 0);
    lv_obj_align(ci_repo_label, LV_ALIGN_TOP_MID, 0, 34);

    ci_ring = lv_arc_create(scr);
    lv_obj_set_size(ci_ring, CI_RING_SIZE, CI_RING_SIZE);
    lv_obj_remove_flag(ci_ring, LV_OBJ_FLAG_CLICKABLE);
    lv_arc_set_rotation(ci_ring, 0);
    lv_arc_set_bg_angles(ci_ring, 0, 360);
    lv_arc_set_angles(ci_ring, 0, 360);
    lv_obj_set_style_arc_width(ci_ring, CI_RING_WIDTH, LV_PART_MAIN);
    lv_obj_set_style_arc_width(ci_ring, CI_RING_WIDTH, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(ci_ring, lv_color_hex(0x202020), LV_PART_MAIN);
    lv_obj_set_style_arc_color(ci_ring, lv_color_hex(ci_pal[0].rim), LV_PART_INDICATOR);
    lv_obj_set_style_opa(ci_ring, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_align(ci_ring, LV_ALIGN_CENTER, 0, CI_ORB_Y);

    ci_center = lv_obj_create(scr);
    lv_obj_set_size(ci_center, CI_CENTER_SIZE, CI_CENTER_SIZE);
    lv_obj_set_style_radius(ci_center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ci_center, lv_color_hex(ci_pal[0].base), 0);
    lv_obj_set_style_border_color(ci_center, lv_color_hex(ci_pal[0].dark), 0);
    lv_obj_set_style_border_width(ci_center, 4, 0);
    lv_obj_set_scrollbar_mode(ci_center, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(ci_center, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(ci_center, LV_ALIGN_CENTER, 0, CI_ORB_Y);
    lv_obj_add_flag(ci_center, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ci_center, ci_orb_clicked_cb, LV_EVENT_CLICKED, NULL);

    /* Stack icon / word / sub-line vertically, centered as a group. */
    lv_obj_set_flex_flow(ci_center, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(ci_center, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(ci_center, 2, 0);

    ci_icon_label = lv_label_create(ci_center);
    lv_label_set_text(ci_icon_label, "");
    lv_obj_set_style_text_color(ci_icon_label, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_48
    lv_obj_set_style_text_font(ci_icon_label, &lv_font_montserrat_48, 0);
#endif

    ci_word_label = lv_label_create(ci_center);
    lv_label_set_text(ci_word_label, "UNKNOWN");
    lv_obj_set_style_text_color(ci_word_label, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(ci_word_label, &lv_font_montserrat_20, 0);
#endif

    ci_sub_label = lv_label_create(ci_center);
    lv_label_set_text(ci_sub_label, "");
    lv_obj_set_style_text_color(ci_sub_label, lv_color_hex(0xEAEAEA), 0);

    ci_title_label = lv_label_create(scr);
    lv_label_set_text(ci_title_label, "Waiting for status...");
    lv_label_set_long_mode(ci_title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ci_title_label, 380);
    lv_obj_set_style_text_align(ci_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ci_title_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(ci_title_label, LV_ALIGN_BOTTOM_MID, 0, -92);

    ci_wf_label = lv_label_create(scr);
    lv_label_set_text(ci_wf_label, "");
    lv_obj_set_style_text_color(ci_wf_label, lv_color_hex(0x808080), 0);
    lv_obj_align(ci_wf_label, LV_ALIGN_BOTTOM_MID, 0, -68);
}

void update_ci_orb(int state, const char* repo, const char* title, const char* wf_branch,
                   const char* runinfo) {
    if (!ci_center)
        return;
    if (state < 0 || state > 3)
        state = 0;

    /* Fresh data arrived — cancel any pending refresh-timeout revert. */
    if (ci_refresh_timer) {
        lv_timer_delete(ci_refresh_timer);
        ci_refresh_timer = NULL;
    }

    const ci_palette_t* p = &ci_pal[state];

    lv_obj_set_style_bg_color(ci_center, lv_color_hex(p->base), 0);
    lv_obj_set_style_border_color(ci_center, lv_color_hex(p->dark), 0);
    lv_obj_set_style_arc_color(ci_ring, lv_color_hex(p->rim), LV_PART_INDICATOR);

    const char* word;
    const char* icon;
    switch (state) {
        case 1:
            word = "RUNNING";
            icon = "";  // dots timer drives the icon slot
            break;
        case 2:
            word = "PASSING";
            icon = LV_SYMBOL_OK;
            break;
        case 3:
            word = "FAILED";
            icon = LV_SYMBOL_CLOSE;
            break;
        default:
            word = "UNKNOWN";
            icon = "";
            break;
    }

    lv_label_set_text(ci_word_label, word);
    lv_label_set_text(ci_sub_label, runinfo ? runinfo : "");
    if (repo && repo[0])
        lv_label_set_text(ci_repo_label, repo);
    if (title && title[0])
        lv_label_set_text(ci_title_label, title);
    lv_label_set_text(ci_wf_label, wf_branch ? wf_branch : "");

    if (state == 1) {  // running — pulsing rim + animated dots
        ci_pulse_base = p->rim;
        ci_pulse_bright = p->rim_bright;
        ci_start_dots();
        ci_start_ring_pulse();
    } else {  // settled — static
        ci_stop_dots();
        ci_stop_ring_pulse();
        lv_label_set_text(ci_icon_label, icon);
    }

    /* Remember the rendered status so a refresh tap can revert to it on timeout. */
    ci_last_state = state;
    strncpy(ci_last_repo, repo ? repo : "", sizeof(ci_last_repo) - 1);
    strncpy(ci_last_title, title ? title : "", sizeof(ci_last_title) - 1);
    strncpy(ci_last_wf, wf_branch ? wf_branch : "", sizeof(ci_last_wf) - 1);
    strncpy(ci_last_info, runinfo ? runinfo : "", sizeof(ci_last_info) - 1);
}
