#include "ci_app.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "LVGL_Example.h"
#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define REPO_LEN 64
#define TITLE_LEN 96
#define WF_LEN 64
#define INFO_LEN 32
#define CMD_DEBOUNCE_MS 500

typedef enum {
    CI_UNKNOWN = 0,
    CI_RUNNING,
    CI_SUCCESS,
    CI_FAILURE,
} ci_state_t;

typedef struct {
    ci_state_t state;
    char repo[REPO_LEN];
    char title[TITLE_LEN];
    char wf_branch[WF_LEN];
    char runinfo[INFO_LEN];
    bool pending;
} ci_buf_t;

static ci_buf_t buf = {0};

static void send_cmd(const char* cmd) {
    static uint32_t last_ms = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_ms < CMD_DEBOUNCE_MS)
        return;
    last_ms = now;

    char msg[32];
    snprintf(msg, sizeof(msg), "CMD:%s\n", cmd);
    app_send(msg);
}

static void init(void) {
    ci_set_cmd_callback(send_cmd);
}

static void create_ui(void) {
    ci_orb();
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
    char state_str[16] = {0}, repo[REPO_LEN] = {0}, title[TITLE_LEN] = {0};
    char wf[WF_LEN] = {0}, info[INFO_LEN] = {0};
    if (sscanf(line, "CI:%15[^|]|%63[^|]|%95[^|]|%63[^|]|%31[^\n]", state_str, repo, title, wf,
               info) < 1)
        return;

    app_lock();
    buf.state = state_from_str(state_str);
    strncpy(buf.repo, repo, REPO_LEN - 1);
    buf.repo[REPO_LEN - 1] = '\0';
    strncpy(buf.title, title, TITLE_LEN - 1);
    buf.title[TITLE_LEN - 1] = '\0';
    strncpy(buf.wf_branch, wf, WF_LEN - 1);
    buf.wf_branch[WF_LEN - 1] = '\0';
    strncpy(buf.runinfo, info, INFO_LEN - 1);
    buf.runinfo[INFO_LEN - 1] = '\0';
    buf.pending = true;
    app_unlock();
}

static void update(void) {
    app_lock();
    if (!buf.pending) {
        app_unlock();
        return;
    }
    ci_state_t state = buf.state;
    char repo[REPO_LEN], title[TITLE_LEN], wf[WF_LEN], info[INFO_LEN];
    strncpy(repo, buf.repo, REPO_LEN);
    strncpy(title, buf.title, TITLE_LEN);
    strncpy(wf, buf.wf_branch, WF_LEN);
    strncpy(info, buf.runinfo, INFO_LEN);
    buf.pending = false;
    app_unlock();

    update_ci_orb((int)state, repo, title, wf, info);
}

const app_t ci_app = {
    .prefix = "CI:",
    .init = init,
    .create_ui = create_ui,
    .parse = parse,
    .update = update,
};
