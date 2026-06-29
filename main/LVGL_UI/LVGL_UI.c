#include "LVGL_UI.h"

#include <stdint.h>
#include <string.h>

#include "ST7701S.h"
#include "ci_app.h"

/* ---- CI / build status orb ------------------------------------------------ */

#define CI_CENTER_SIZE 160
#define CI_RING_SIZE 200
#define CI_RING_WIDTH 8
#define CI_ORB_Y (6)

/* Horizontal offset to compensate for RGB panel timing shift. Negative = left. */
#define DISP_H_OFFSET (0)

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

/* ===== WiFi setup (SoftAP provisioning) =================================== */

/* Must match the SoftAP brought up by wifi_sta_start_ap(). */
#define AP_SSID_TXT "CI-Orb-Setup"
#define AP_PASS_TXT "ciorbsetup"
#define AP_URL_TXT "http://192.168.4.1"
/* Phone-camera WiFi-join payload: scanning it offers to join the hotspot. */
#define AP_WIFI_QR "WIFI:T:WPA;S:" AP_SSID_TXT ";P:" AP_PASS_TXT ";;"

static lv_obj_t* set_qr;
static lv_obj_t* set_qr_cap;
static lv_obj_t* set_next_lbl;
static int set_qr_stage; /* 0 = join hotspot, 1 = open config URL */

/* Render either the WiFi-join QR (step 1) or the config-URL QR (step 2). */
static void set_qr_show(int stage) {
    set_qr_stage = stage;
    if (stage == 0) {
        lv_qrcode_set_data(set_qr, AP_WIFI_QR);
        lv_label_set_text(set_qr_cap, "1. Scan to join " AP_SSID_TXT);
        lv_label_set_text(set_next_lbl, "URL " LV_SYMBOL_RIGHT);
    } else {
        lv_qrcode_set_data(set_qr, AP_URL_TXT);
        lv_label_set_text(set_qr_cap, "2. Scan to open " AP_URL_TXT);
        lv_label_set_text(set_next_lbl, LV_SYMBOL_LEFT " WiFi");
    }
}

static void set_back_cb(lv_event_t* e) {
    (void)e;
    set_ip_label = NULL;
    set_qr = NULL;
    set_qr_cap = NULL;
    set_next_lbl = NULL;
    ci_show_orb();
}

static void set_next_cb(lv_event_t* e) {
    (void)e;
    set_qr_show(set_qr_stage == 0 ? 1 : 0);
}

/* No-typing settings screen: scan a QR to join the device hotspot, tap Next for
 * a second QR to the config page. The full config form lives in web_server.c. */
void ci_settings_screen(void) {
    if (s_settings_scr) {
        lv_obj_delete(s_settings_scr);
        s_settings_scr = NULL;
    }
    set_ip_label = NULL;

    /* Ensure the hotspot + web server are up so the QR codes actually work,
     * including when reached via the gear button after WiFi is connected. */
    ci_app_start_provisioning();

    lv_obj_t* scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(scr, 0, 0);
    s_settings_scr = scr;
    lv_screen_load(scr);

    lv_obj_t* root = make_root(scr);

    lv_obj_t* title = lv_label_create(root);
    lv_label_set_text(title, "WiFi Setup");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
#if LV_FONT_MONTSERRAT_20
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
#endif
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 40);

    set_qr = lv_qrcode_create(root);
    lv_qrcode_set_size(set_qr, 180);
    lv_qrcode_set_dark_color(set_qr, lv_color_black());
    lv_qrcode_set_light_color(set_qr, lv_color_white());
    lv_qrcode_set_quiet_zone(set_qr, true);
    lv_obj_align(set_qr, LV_ALIGN_CENTER, 0, -30);

    set_qr_cap = lv_label_create(root);
    lv_obj_set_style_text_color(set_qr_cap, lv_color_hex(0xCCCCCC), 0);
    lv_obj_set_style_text_align(set_qr_cap, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(set_qr_cap, 300);
    lv_obj_align_to(set_qr_cap, set_qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    set_ip_label = lv_label_create(root);
    lv_label_set_text(set_ip_label, "Connect to WiFi to get IP");
    lv_obj_set_style_text_color(set_ip_label, lv_color_hex(0x607D8B), 0);
    lv_obj_set_style_text_align(set_ip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(set_ip_label, 300);
    lv_obj_align_to(set_ip_label, set_qr_cap, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    lv_obj_t* btn_row = lv_obj_create(root);
    lv_obj_set_size(btn_row, 300, LV_SIZE_CONTENT);
    lv_obj_set_scrollbar_mode(btn_row, LV_SCROLLBAR_MODE_OFF);
    lv_obj_remove_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_set_style_pad_all(btn_row, 0, 0);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_align(btn_row, LV_ALIGN_BOTTOM_MID, 0, -55);

    lv_obj_t* back = lv_button_create(btn_row);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x3A3A3A), 0);
    lv_obj_add_event_cb(back, set_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t* back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, "Back");
    lv_obj_center(back_lbl);

    lv_obj_t* next = lv_button_create(btn_row);
    lv_obj_set_style_bg_color(next, lv_color_hex(0x1A3A6A), 0);
    lv_obj_add_event_cb(next, set_next_cb, LV_EVENT_CLICKED, NULL);
    set_next_lbl = lv_label_create(next);
    lv_obj_center(set_next_lbl);

    set_qr_show(0);
}
