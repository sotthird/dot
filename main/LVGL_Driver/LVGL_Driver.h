#pragma once

#include "CST820.h"
#include "ST7701S.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lvgl.h"

#define EXAMPLE_LVGL_TICK_PERIOD_MS 2

void LVGL_Init(void);
