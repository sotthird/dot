#pragma once

#include "lvgl.h"

void ci_orb(void);
void ci_show_orb(void);
void ci_set_watch_branch(const char* branch);
void ci_settings_screen(const char* ssid, const char* pass);
void update_ci_orb(int state, const char* repo, const char* title, const char* wf_branch,
                   const char* runinfo);

/* Called from ci_app_update() when a WiFi scan completes. ssids[i] are NUL-
 * terminated strings; count is the number of entries. No-op if the settings
 * screen is not currently displayed. */
void update_ssid_dropdown(const char* const* ssids, uint16_t count);

/* Set or clear the URL label at the bottom of the orb screen.
 * Pass an empty string to hide the label. */
void ci_ui_set_url(const char* url);
