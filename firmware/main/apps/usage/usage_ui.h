#pragma once

#include "lvgl.h"

/* Claude Code token usage: today in large type, the last 5 hours and 7 days beneath, each as a
 * percentage of the host's budget with the token count below. A percentage of "-1" means no
 * budget is set, and the token count is shown alone. */
void usage_ui_create(lv_obj_t* screen);
void usage_ui_update(const char* today, const char* five_hours, const char* week,
                     const char* messages, const char* pct_5h, const char* pct_7d, const char* reset_5h,
                     const char* reset_7d);
