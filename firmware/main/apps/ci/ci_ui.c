#include "ci_ui.h"

#include <string.h>

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
static ci_ui_cmd_cb_t ci_cmd_cb = NULL;

static lv_timer_t* ci_dots_timer = NULL;
static uint32_t ci_pulse_base = 0x555555;
static uint32_t ci_pulse_bright = 0x777777;

/* Last rendered status, kept so a refresh can revert if no fresh data arrives. */
static lv_timer_t* ci_refresh_timer = NULL;
static ci_status_t ci_last;

#define CI_REFRESH_BASE 0x3A6EA5
#define CI_REFRESH_BRIGHT 0x7FB3E8
#define CI_REFRESH_DARK 0x244A6E

static void ci_show_refreshing(void);

/* Fire on press-down (not click/release) so the refresh feedback feels instant
 * instead of waiting for the finger to lift. Ignore presses while a refresh is
 * already in flight, so holding the orb down doesn't repeatedly retrigger it. */
static void ci_orb_pressed_cb(lv_event_t* e) {
    (void)e;
    if (ci_refresh_timer)
        return;
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
    ci_ui_update(&ci_last);
}

/* Immediate on-tap feedback: blue pulsing ring + animated dots while we wait for
 * the host to re-fetch. The next ci_ui_update() cancels this and shows the result. */
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

void ci_ui_create(lv_obj_t* scr, ci_ui_cmd_cb_t on_cmd) {
    ci_cmd_cb = on_cmd;

    lv_obj_set_style_bg_color(scr, lv_color_hex(0x121212), 0);

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
    lv_obj_add_event_cb(ci_center, ci_orb_pressed_cb, LV_EVENT_PRESSED, NULL);

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
    lv_label_set_long_mode(ci_sub_label, LV_LABEL_LONG_DOT);
    lv_obj_set_width(ci_sub_label, CI_CENTER_SIZE - 30);
    lv_obj_set_style_text_align(ci_sub_label, LV_TEXT_ALIGN_CENTER, 0);
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

void ci_ui_update(const ci_status_t* s) {
    if (!ci_center)
        return;
    const ci_state_t state = s->state <= CI_FAILURE ? s->state : CI_UNKNOWN;

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
        case CI_RUNNING:
            word = "RUNNING";
            icon = "";  // dots timer drives the icon slot
            break;
        case CI_SUCCESS:
            word = "PASSING";
            icon = LV_SYMBOL_OK;
            break;
        case CI_FAILURE:
            word = "FAILED";
            icon = LV_SYMBOL_CLOSE;
            break;
        default:
            word = "UNKNOWN";
            icon = "";
            break;
    }

    lv_label_set_text(ci_word_label, word);
    lv_label_set_text(ci_sub_label, s->runinfo);
    if (s->repo[0])
        lv_label_set_text(ci_repo_label, s->repo);
    if (s->title[0])
        lv_label_set_text(ci_title_label, s->title);
    lv_label_set_text(ci_wf_label, s->wf_branch);

    if (state == CI_RUNNING) {  // running: pulsing rim and animated dots
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
    ci_last = *s;
}

void ci_ui_suspend(void) {
    if (ci_refresh_timer) {
        lv_timer_delete(ci_refresh_timer);
        ci_refresh_timer = NULL;
    }
    ci_stop_dots();
    ci_stop_ring_pulse();
}

void ci_ui_resume(void) {
    ci_ui_update(&ci_last);
}
