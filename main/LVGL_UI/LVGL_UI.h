#pragma once

#include "lvgl.h"

void ci_orb(void);
void ci_show_orb(void);
void ci_set_watch_branch(const char* branch);
void ci_settings_screen(void);
void update_ci_orb(int state, const char* repo, const char* title, const char* wf_branch,
                   const char* runinfo);

/* Set or clear the URL label at the bottom of the orb screen.
 * Pass an empty string to hide the label. */
void ci_ui_set_url(const char* url);
