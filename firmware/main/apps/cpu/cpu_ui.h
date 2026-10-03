#pragma once

#include "lvgl.h"

/* Radial gauge of host CPU usage. */
void cpu_ui_create(void);
void cpu_ui_update(float cpu_pct);
