#pragma once

#include "lvgl.h"

typedef enum {
    CI_UNKNOWN = 0,
    CI_RUNNING,
    CI_SUCCESS,
    CI_FAILURE,
} ci_state_t;

typedef struct {
    ci_state_t state;
    char repo[64];
    char title[96];
    char wf_branch[64];
    char runinfo[32];
} ci_status_t;

/* Called with "ci_refresh" when the orb is tapped. */
typedef void (*ci_ui_cmd_cb_t)(const char* cmd);

/* Whole-screen status orb: green passing, amber and pulsing while running, red on failure. */
void ci_ui_create(ci_ui_cmd_cb_t on_cmd);
void ci_ui_update(const ci_status_t* status);
