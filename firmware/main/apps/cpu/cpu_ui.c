#include "cpu_ui.h"

#include <stdio.h>

static lv_obj_t* cpu_arc;
static lv_obj_t* cpu_pct_label;

void cpu_ui_create(lv_obj_t* scr) {
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

void cpu_ui_update(float cpu_pct) {
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
