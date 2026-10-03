#include "ci.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "ci_ui.h"

static ci_status_t status;
static bool status_pending;

static void create_ui(void) {
    ci_ui_create(app_send_cmd);
}

static ci_state_t state_from_str(const char* s) {
    if (strcmp(s, "running") == 0)
        return CI_RUNNING;
    if (strcmp(s, "success") == 0)
        return CI_SUCCESS;
    if (strcmp(s, "failure") == 0)
        return CI_FAILURE;
    return CI_UNKNOWN;
}

static void parse(const char* line) {
    char state[16] = {0};
    ci_status_t s = {0};

    if (sscanf(line, "CI:%15[^|]|%63[^|]|%95[^|]|%63[^|]|%31[^\n]", state, s.repo, s.title,
               s.wf_branch, s.runinfo) < 1)
        return;
    s.state = state_from_str(state);

    app_lock();
    status = s;
    status_pending = true;
    app_unlock();
}

static void update(void) {
    app_lock();
    bool pending = status_pending;
    ci_status_t s = status;
    status_pending = false;
    app_unlock();

    if (pending)
        ci_ui_update(&s);
}

const app_t ci_app = {
    .prefix = "CI:",
    .create_ui = create_ui,
    .parse = parse,
    .update = update,
};
