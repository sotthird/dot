#pragma once

#include "lvgl.h"

/* Radial gauge of host CPU usage. */
void cpu_ui_create(lv_obj_t* screen);
void cpu_ui_update(float cpu_pct);
