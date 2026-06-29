#include "LVGL_UI.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ST7701S.h"
#include "ci_app.h"

/* ---- CI / build status orb ------------------------------------------------ */

#define CI_CENTER_SIZE 210
#define CI_RING_SIZE 250
#define CI_RING_WIDTH 16
#define CI_ORB_Y (10)

/* Horizontal offset to compensate for RGB panel timing shift. Negative = left. */
#define DISP_H_OFFSET (-85)

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

/* Branch options for the orb dropdown: index 0 = all branches (empty string). */
static const char* const BRANCH_OPTS[] = {"", "main", "develop", "staging"};
#define BRANCH_OPTS_COUNT 4
#define BRANCH_DD_STR "all branches\nmain\ndevelop\nstaging"

static lv_obj_t* ci_ring;
static lv_obj_t* ci_center;
static lv_obj_t* ci_icon_label;
static lv_obj_t* ci_word_label;
static lv_obj_t* ci_sub_label;
static lv_obj_t* ci_repo_label;
static lv_obj_t* ci_title_label;
static lv_obj_t* ci_wf_label;
static lv_obj_t* ci_branch_dd;
static lv_obj_t* ci_url_label;
static lv_obj_t* set_ip_label;
static lv_obj_t* s_orb_scr = NULL;
static lv_obj_t* s_settings_scr = NULL;
static char ci_watch_branch[64] = "";

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

static void ci_gear_cb(lv_event_t* e) {
    (void)e;
    ci_app_open_settings();
}

void ci_set_watch_branch(const char* branch) {
    strncpy(ci_watch_branch, branch ? branch : "", sizeof(ci_watch_branch) - 1);
    ci_watch_branch[sizeof(ci_watch_branch) - 1] = '\0';
    if (ci_branch_dd) {
        uint16_t idx = 0;
        for (int i = 0; i < BRANCH_OPTS_COUNT; i++) {
            if (strcmp(ci_watch_branch, BRANCH_OPTS[i]) == 0) {
                idx = (uint16_t)i;
                break;
            }
        }
        lv_dropdown_set_selected(ci_branch_dd, idx);
    }
}

/* Fire on press-down (not click/release) so the refresh feedback feels instant
 * instead of waiting for the finger to lift. Ignore presses while a refresh is
 * already in flight, so holding the orb down doesn't repeatedly retrigger it. */
static void ci_orb_pressed_cb(lv_event_t* e) {
    (void)e;
    if (ci_refresh_timer)
        return;
    ci_show_refreshing();
    ci_app_request_refresh();
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

static void ci_refresh_timeout_cb(lv_timer_t* t) {
    (void)t;
    ci_refresh_timer = NULL;
    update_ci_orb(ci_last_state, ci_last_repo, ci_last_title, ci_last_wf, ci_last_info);
}

/* Immediate on-tap feedback: blue pulsing ring + animated dots while we wait
 * for the poll task to re-fetch. The next update_ci_orb() cancels this. */
static void ci_show_refreshing(void) {
    if (ci_word_label)
        lv_label_set_text(ci_word_label, "REFRESH");
    if (ci_sub_label)
        lv_label_set_text(ci_sub_label, "");

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

static void ci_branch_dd_cb(lv_event_t* e) {
    lv_obj_t* dd = lv_event_get_target(e);
    uint16_t sel = lv_dropdown_get_selected(dd);
    const char* branch = (sel < BRANCH_OPTS_COUNT) ? BRANCH_OPTS[sel] : "";
    strncpy(ci_watch_branch, branch, sizeof(ci_watch_branch) - 1);
    ci_watch_branch[sizeof(ci_watch_branch) - 1] = '\0';
    ci_app_set_branch(branch);
}

void ci_ui_set_url(const char* url) {
    if (ci_url_label) {
        if (url && url[0]) {
            lv_label_set_text(ci_url_label, url);
            lv_obj_remove_flag(ci_url_label, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ci_url_label, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (set_ip_label) {
        if (url && url[0])
            lv_label_set_text(set_ip_label, url);
        else
            lv_label_set_text(set_ip_label, "Connect to WiFi to get IP");
    }
}

void ci_show_orb(void) {
    if (s_orb_scr)
        lv_screen_load(s_orb_scr);
}

/* Create a transparent full-size container shifted by DISP_H_OFFSET so all
 * child widgets use normal centered alignment with no per-widget x fudge. */
static lv_obj_t* make_root(lv_obj_t* scr) {
    lv_obj_t* root = lv_obj_create(scr);
    lv_obj_set_size(root, EXAMPLE_LCD_H_RES, EXAMPLE_LCD_V_RES);
    lv_obj_set_pos(root, DISP_H_OFFSET, 0);
    lv_obj_set_style_bg_opa(root, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(root, 0, 0);
    lv_obj_set_style_pad_all(root, 0, 0);
    lv_obj_remove_flag(root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(root, LV_SCROLLBAR_MODE_OFF);
    return root;
}

void ci_orb(void) {
    if (s_orb_scr) {
        lv_screen_load(s_orb_scr);
        return;
    }
    ci_url_label = NULL;
    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    s_orb_scr = scr;
    lv_screen_load(scr);

    lv_obj_t* root = make_root(scr);

    ci_repo_label = lv_label_create(root);
    lv_label_set_text(ci_repo_label, "CI Status");
    lv_obj_set_style_text_color(ci_repo_label, lv_color_hex(0xB3B3B3), 0);
    lv_obj_align(ci_repo_label, LV_ALIGN_TOP_MID, 0, 20);

    ci_ring = lv_arc_create(root);
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

    ci_center = lv_obj_create(root);
    lv_obj_set_size(ci_center, CI_CENTER_SIZE, CI_CENTER_SIZE);
    lv_obj_set_style_radius(ci_center, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(ci_center, lv_color_hex(ci_pal[0].base), 0);
    lv_obj_set_style_border_color(ci_center, lv_color_hex(ci_pal[0].dark), 0);
    lv_obj_set_style_border_width(ci_center, 4, 0);
    lv_obj_set_scrollbar_mode(ci_center, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(ci_center, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(ci_center, LV_ALIGN_CENTER, 0, CI_ORB_Y);
    lv_obj_add_flag(ci_center, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(ci_center, ci_orb_pressed_cb, LV_EVENT_PRESSED, NULL);

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

    ci_title_label = lv_label_create(root);
    lv_label_set_text(ci_title_label, "Waiting for status...");
    lv_label_set_long_mode(ci_title_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ci_title_label, 300);
    lv_obj_set_style_text_align(ci_title_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ci_title_label, lv_color_hex(0xE0E0E0), 0);
    lv_obj_align(ci_title_label, LV_ALIGN_BOTTOM_MID, 0, -75);

    ci_wf_label = lv_label_create(root);
    lv_label_set_text(ci_wf_label, "");
    lv_obj_set_style_text_color(ci_wf_label, lv_color_hex(0x808080), 0);
    lv_obj_align(ci_wf_label, LV_ALIGN_BOTTOM_MID, 0, -50);

    ci_url_label = lv_label_create(root);
    lv_label_set_text(ci_url_label, "");
    lv_label_set_long_mode(ci_url_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ci_url_label, 220);
    lv_obj_set_style_text_align(ci_url_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(ci_url_label, lv_color_hex(0x90A4AE), 0);
    lv_obj_align_to(ci_url_label, ci_wf_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 4);
    lv_obj_add_flag(ci_url_label, LV_OBJ_FLAG_HIDDEN);

    ci_branch_dd = lv_dropdown_create(root);
    lv_dropdown_set_options(ci_branch_dd, BRANCH_DD_STR);
    lv_obj_set_width(ci_branch_dd, 180);
    lv_obj_align(ci_branch_dd, LV_ALIGN_TOP_MID, 0, 62);
    lv_obj_set_style_bg_color(ci_branch_dd, lv_color_hex(0x1E2A3A), 0);
    lv_obj_set_style_text_color(ci_branch_dd, lv_color_hex(0x8AB4F8), 0);
    lv_obj_set_style_border_color(ci_branch_dd, lv_color_hex(0x3A5A8A), 0);
    lv_obj_set_style_border_width(ci_branch_dd, 1, 0);
    lv_obj_add_event_cb(ci_branch_dd, ci_branch_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);
    ci_set_watch_branch(ci_watch_branch);

    lv_obj_t* gear = lv_button_create(root);
    lv_obj_set_size(gear, 44, 44);
    lv_obj_set_style_radius(gear, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(gear, lv_color_hex(0x2A2A2A), 0);
    lv_obj_align_to(gear, ci_branch_dd, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_add_event_cb(gear, ci_gear_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* gear_lbl = lv_label_create(gear);
    lv_label_set_text(gear_lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(gear_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(gear_lbl);
}

void update_ci_orb(int state, const char* repo, const char* title, const char* wf_branch,
                   const char* runinfo) {
    if (!ci_center)
        return;
    if (state < 0 || state > 3)
        state = 0;

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
            icon = "";
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

    if (state == 1) {
        ci_pulse_base = p->rim;
        ci_pulse_bright = p->rim_bright;
        ci_start_dots();
        ci_start_ring_pulse();
    } else {
        ci_stop_dots();
        ci_stop_ring_pulse();
        lv_label_set_text(ci_icon_label, icon);
    }

    ci_last_state = state;
    strncpy(ci_last_repo, repo ? repo : "", sizeof(ci_last_repo) - 1);
    strncpy(ci_last_title, title ? title : "", sizeof(ci_last_title) - 1);
    strncpy(ci_last_wf, wf_branch ? wf_branch : "", sizeof(ci_last_wf) - 1);
    strncpy(ci_last_info, runinfo ? runinfo : "", sizeof(ci_last_info) - 1);
}

/* ===== Settings form ====================================================== */

#define SCAN_OPTS_MAX 600 /* SCAN_MAX * 33 + separators + "(enter manually)\0" */

static lv_obj_t* set_kb;
static lv_obj_t* set_ssid_dd;
static lv_obj_t* set_ssid_ta;
static lv_obj_t* set_btn_row;
static lv_obj_t* set_form;
static lv_obj_t* set_title_obj;
static lv_obj_t* set_float_ta;
static lv_obj_t* set_float_lbl;
static lv_obj_t* set_active_ta;
static char set_ssid_current[33]; /* saved SSID for pre-selection after scan */
static lv_obj_t* set_pass_ta;

static void set_float_close(void) {
    if (set_active_ta && set_float_ta)
        lv_textarea_set_text(set_active_ta, lv_textarea_get_text(set_float_ta));
    set_active_ta = NULL;
    if (set_kb) {
        lv_keyboard_set_textarea(set_kb, NULL);
        lv_obj_add_flag(set_kb, LV_OBJ_FLAG_HIDDEN);
    }
    if (set_float_ta)
        lv_obj_add_flag(set_float_ta, LV_OBJ_FLAG_HIDDEN);
    if (set_float_lbl)
        lv_obj_add_flag(set_float_lbl, LV_OBJ_FLAG_HIDDEN);
    if (set_form)
        lv_obj_remove_flag(set_form, LV_OBJ_FLAG_HIDDEN);
    if (set_btn_row)
        lv_obj_remove_flag(set_btn_row, LV_OBJ_FLAG_HIDDEN);
    if (set_title_obj)
        lv_obj_remove_flag(set_title_obj, LV_OBJ_FLAG_HIDDEN);
}

static void set_float_ta_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL || code == LV_EVENT_DEFOCUSED)
        set_float_close();
}

static void set_ta_event_cb(lv_event_t* e) {
    if (lv_event_get_code(e) != LV_EVENT_FOCUSED)
        return;
    lv_obj_t* ta = lv_event_get_target(e);
    set_active_ta = ta;

    const char* field_name = "Enter text";
    bool is_pw = false;
    if (ta == set_ssid_ta) {
        field_name = "WiFi Network";
    }
    if (ta == set_pass_ta) {
        field_name = "WiFi Password";
        is_pw = true;
    }

    lv_label_set_text(set_float_lbl, field_name);
    lv_textarea_set_password_mode(set_float_ta, is_pw);
    lv_textarea_set_text(set_float_ta, lv_textarea_get_text(ta));

    lv_obj_remove_flag(set_float_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(set_float_ta, LV_OBJ_FLAG_HIDDEN);

    if (set_form)
        lv_obj_add_flag(set_form, LV_OBJ_FLAG_HIDDEN);
    if (set_btn_row)
        lv_obj_add_flag(set_btn_row, LV_OBJ_FLAG_HIDDEN);
    if (set_title_obj)
        lv_obj_add_flag(set_title_obj, LV_OBJ_FLAG_HIDDEN);

    lv_keyboard_set_textarea(set_kb, set_float_ta);
    lv_buttonmatrix_set_button_ctrl_all(set_kb, LV_BUTTONMATRIX_CTRL_NO_REPEAT);
    lv_obj_remove_flag(set_kb, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t* set_add_field(lv_obj_t* parent, const char* label, bool password,
                               const char* value) {
    lv_obj_t* lbl = lv_label_create(parent);
    lv_label_set_text(lbl, label);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xB3B3B3), 0);

    lv_obj_t* ta = lv_textarea_create(parent);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_password_mode(ta, password);
    lv_textarea_set_text(ta, value ? value : "");
    lv_obj_set_width(ta, lv_pct(100));
    lv_obj_add_event_cb(ta, set_ta_event_cb, LV_EVENT_ALL, NULL);
    return ta;
}

static void set_ssid_dd_cb(lv_event_t* e) {
    (void)e;
    if (!set_ssid_dd || !set_ssid_ta)
        return;
    char buf[33];
    lv_dropdown_get_selected_str(set_ssid_dd, buf, sizeof(buf));
    lv_textarea_set_text(set_ssid_ta, buf);
}

static const char* get_ssid_value(void) {
    return set_ssid_ta ? lv_textarea_get_text(set_ssid_ta) : "";
}

void update_ssid_dropdown(const char* const* ssids, uint16_t count) {
    if (!set_ssid_dd || count == 0)
        return;

    static char opts[SCAN_OPTS_MAX];
    opts[0] = '\0';
    for (uint16_t i = 0; i < count; i++) {
        if (i > 0)
            strncat(opts, "\n", SCAN_OPTS_MAX - strlen(opts) - 1);
        strncat(opts, ssids[i], SCAN_OPTS_MAX - strlen(opts) - 1);
    }
    lv_dropdown_set_options(set_ssid_dd, opts);

    /* Pre-select the saved SSID if it appears in the scan results. */
    for (uint16_t i = 0; i < count; i++) {
        if (strcmp(ssids[i], set_ssid_current) == 0) {
            lv_dropdown_set_selected(set_ssid_dd, i);
            break;
        }
    }
}

static void set_save_cb(lv_event_t* e) {
    (void)e;
    const char* ssid = get_ssid_value();
    const char* pass = lv_textarea_get_text(set_pass_ta);
    set_ssid_dd = NULL;
    set_ssid_ta = NULL;
    set_btn_row = NULL;
    set_form = NULL;
    set_title_obj = NULL;
    set_float_ta = NULL;
    set_float_lbl = NULL;
    set_active_ta = NULL;
    set_pass_ta = NULL;
    ci_app_connect_wifi(ssid, pass);
}

static void set_scan_cb(lv_event_t* e) {
    (void)e;
    if (set_ssid_dd)
        lv_dropdown_set_options(set_ssid_dd, "Scanning...");
    ci_app_request_scan();
}

static void set_back_cb(lv_event_t* e) {
    (void)e;
    set_ssid_dd = NULL;
    set_ssid_ta = NULL;
    set_btn_row = NULL;
    set_form = NULL;
    set_title_obj = NULL;
    set_float_ta = NULL;
    set_float_lbl = NULL;
    set_active_ta = NULL;
    ci_show_orb();
}

void ci_settings_screen(const char* ssid, const char* pass) {
    if (s_settings_scr) {
        lv_obj_delete(s_settings_scr);
        s_settings_scr = NULL;
    }
    set_ssid_dd = NULL;
    set_ssid_ta = NULL;
    set_btn_row = NULL;
    set_form = NULL;
    set_title_obj = NULL;
    set_float_ta = NULL;
    set_float_lbl = NULL;
    set_active_ta = NULL;
    set_ip_label = NULL;
    strncpy(set_ssid_current, ssid ? ssid : "", sizeof(set_ssid_current) - 1);
    set_ssid_current[sizeof(set_ssid_current) - 1] = '\0';

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(scr, LV_DIR_NONE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    s_settings_scr = scr;
    lv_screen_load(scr);

    lv_obj_t* root = make_root(scr);

    set_title_obj = lv_label_create(root);
    lv_label_set_text(set_title_obj, "Settings");
    lv_obj_set_style_text_color(set_title_obj, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(set_title_obj, LV_ALIGN_TOP_MID, 0, 25);

    set_form = lv_obj_create(root);
    lv_obj_t* form = set_form;
    lv_obj_set_size(form, 300, 280);
    lv_obj_align(form, LV_ALIGN_CENTER, 0, -10);
    lv_obj_set_scrollbar_mode(form, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(form, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(form, LV_DIR_NONE);
    lv_obj_set_style_bg_opa(form, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_style_pad_all(form, 0, 0);
    lv_obj_set_style_pad_row(form, 4, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);

    /* SSID: editable text area + scan dropdown to fill it from scan results. */
    lv_obj_t* ssid_lbl = lv_label_create(form);
    lv_label_set_text(ssid_lbl, "WiFi SSID");
    lv_obj_set_style_text_color(ssid_lbl, lv_color_hex(0xB3B3B3), 0);

    set_ssid_ta = lv_textarea_create(form);
    lv_textarea_set_one_line(set_ssid_ta, true);
    lv_textarea_set_text(set_ssid_ta, ssid ? ssid : "");
    lv_textarea_set_placeholder_text(set_ssid_ta, "Network name");
    lv_obj_set_width(set_ssid_ta, lv_pct(100));
    lv_obj_add_event_cb(set_ssid_ta, set_ta_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t* ssid_row = lv_obj_create(form);
    lv_obj_set_size(ssid_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(ssid_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(ssid_row, 0, 0);
    lv_obj_set_style_pad_all(ssid_row, 0, 0);
    lv_obj_set_style_pad_column(ssid_row, 4, 0);
    lv_obj_set_scrollbar_mode(ssid_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(ssid_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(ssid_row, LV_DIR_NONE);
    lv_obj_set_flex_flow(ssid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(ssid_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    set_ssid_dd = lv_dropdown_create(ssid_row);
    lv_obj_set_flex_grow(set_ssid_dd, 1);
    lv_dropdown_set_options(set_ssid_dd, "(tap \xEF\x80\xA1 to scan)");
    lv_obj_add_event_cb(set_ssid_dd, set_ssid_dd_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_t* scan_btn = lv_button_create(ssid_row);
    lv_obj_set_size(scan_btn, 38, 38);
    lv_obj_set_style_bg_color(scan_btn, lv_color_hex(0x1A3A6A), 0);
    lv_obj_add_event_cb(scan_btn, set_scan_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* scan_lbl = lv_label_create(scan_btn);
    lv_label_set_text(scan_lbl, LV_SYMBOL_REFRESH);
    lv_obj_set_style_text_color(scan_lbl, lv_color_hex(0xCCCCCC), 0);
    lv_obj_center(scan_lbl);

    set_pass_ta = set_add_field(form, "WiFi Password", true, pass);

    set_ip_label = lv_label_create(form);
    lv_label_set_text(set_ip_label, "Connect to WiFi to get IP");
    lv_obj_set_style_text_color(set_ip_label, lv_color_hex(0x607D8B), 0);
    lv_obj_set_style_text_align(set_ip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(set_ip_label, lv_pct(100));

    set_btn_row = lv_obj_create(root);
    lv_obj_t* btn_row = set_btn_row;
    lv_obj_set_size(btn_row, 260, LV_SIZE_CONTENT);
    lv_obj_set_scrollbar_mode(btn_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(btn_row, LV_DIR_NONE);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align_to(btn_row, form, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    lv_obj_t* back = lv_button_create(btn_row);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x3A3A3A), 0);
    lv_obj_add_event_cb(back, set_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, "Back");
    lv_obj_center(back_lbl);

    lv_obj_t* save = lv_button_create(btn_row);
    lv_obj_set_style_bg_color(save, lv_color_hex(0x2E7D32), 0);
    lv_obj_add_event_cb(save, set_save_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* save_lbl = lv_label_create(save);
    lv_label_set_text(save_lbl, LV_SYMBOL_SAVE "  Save");
    lv_obj_center(save_lbl);

    set_kb = lv_keyboard_create(root);
    lv_obj_set_size(set_kb, 420, 300);
    lv_obj_align(set_kb, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_buttonmatrix_set_button_ctrl_all(set_kb, LV_BUTTONMATRIX_CTRL_NO_REPEAT);
    lv_obj_add_flag(set_kb, LV_OBJ_FLAG_HIDDEN);

    set_float_lbl = lv_label_create(root);
    lv_label_set_text(set_float_lbl, "");
    lv_obj_set_style_text_color(set_float_lbl, lv_color_hex(0xB3B3B3), 0);
    lv_obj_add_flag(set_float_lbl, LV_OBJ_FLAG_HIDDEN);

    set_float_ta = lv_textarea_create(root);
    lv_textarea_set_one_line(set_float_ta, true);
    lv_obj_set_width(set_float_ta, 300);
    lv_obj_add_event_cb(set_float_ta, set_float_ta_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_flag(set_float_ta, LV_OBJ_FLAG_HIDDEN);

    lv_obj_align(set_float_ta, LV_ALIGN_BOTTOM_MID, 0, -330);
    lv_obj_align_to(set_float_lbl, set_float_ta, LV_ALIGN_OUT_TOP_LEFT, 0, -2);
}
