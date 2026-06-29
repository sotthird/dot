#pragma once

void ci_app_init(void);
void ci_app_create_ui(void);
void ci_app_update(void);

/* Called by LVGL_UI event handlers directly (no callback indirection). */
void ci_app_request_refresh(void);
void ci_app_open_settings(void);
void ci_app_connect_wifi(const char* ssid, const char* pass);
void ci_app_set_branch(const char* branch);
void ci_app_request_scan(void);
