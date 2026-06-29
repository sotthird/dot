#include "ci_app.h"

#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include "LVGL_UI.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "github_ci.h"
#include "mdns.h"
#include "settings.h"
#include "web_server.h"
#include "wifi_sta.h"

#define TAG "ci_app"
#define TITLE_LEN 96
#define INFO_LEN 32

#define POLL_INTERVAL_MS 30000

typedef struct {
    int state;
    char repo[SET_REPO_MAX];
    char title[TITLE_LEN];
    char wf_branch[SET_BRANCH_MAX];
    char runinfo[INFO_LEN];
    bool pending;
} ci_buf_t;

static ci_buf_t s_buf = {0};
static SemaphoreHandle_t s_mux = NULL;
static TaskHandle_t s_poll_task_handle = NULL;
static bool s_poll_started = false;

/* IP label state — written by on_wifi_connected/disconnected, read by ci_app_update. */
static char s_ip_url[48] = "";
static bool s_ip_pending = false;

/* One-shot flag: web server and mDNS are only initialised once per power-on. */
static bool s_web_started = false;

static void lock(void) {
    xSemaphoreTake(s_mux, portMAX_DELAY);
}
static void unlock(void) {
    xSemaphoreGive(s_mux);
}

static void store(int state, const char* repo, const char* title, const char* wf,
                  const char* info) {
    lock();
    s_buf.state = state;
    strncpy(s_buf.repo, repo ? repo : "", SET_REPO_MAX - 1);
    s_buf.repo[SET_REPO_MAX - 1] = '\0';
    strncpy(s_buf.title, title ? title : "", TITLE_LEN - 1);
    s_buf.title[TITLE_LEN - 1] = '\0';
    strncpy(s_buf.wf_branch, wf ? wf : "", SET_BRANCH_MAX - 1);
    s_buf.wf_branch[SET_BRANCH_MAX - 1] = '\0';
    strncpy(s_buf.runinfo, info ? info : "", INFO_LEN - 1);
    s_buf.runinfo[INFO_LEN - 1] = '\0';
    s_buf.pending = true;
    unlock();
}

/* Returns the number of extra ms to wait before the next fetch (0 = normal).
 * Snapshots settings under the mutex so a concurrent web-dashboard save cannot
 * produce a torn read of token/repo/branch. */
static uint32_t fetch_once(void) {
    settings_t snap;
    lock();
    snap = *settings_get();
    unlock();

    if (!wifi_sta_is_connected()) {
        store(0, snap.repo, "", "WiFi: connecting...", "");
        return 0;
    }
    gh_result_t r;
    if (github_ci_fetch(snap.token, snap.repo, snap.branch, &r)) {
        store((int)r.state, r.repo, r.title, r.wf_branch, r.runinfo);
    } else {
        store(0, snap.repo, "", "Fetch failed", "");
        if (r.retry_after_s)
            return r.retry_after_s * 1000;
    }
    return 0;
}

static void poll_timer_cb(TimerHandle_t t) {
    (void)t;
    if (s_poll_task_handle)
        xTaskNotify(s_poll_task_handle, 0, eNoAction);
}

static void poll_task(void* arg) {
    (void)arg;
    s_poll_task_handle = xTaskGetCurrentTaskHandle();
    TimerHandle_t timer =
        xTimerCreate("ci_poll", pdMS_TO_TICKS(POLL_INTERVAL_MS), pdFALSE, NULL, poll_timer_cb);
    uint32_t backoff_ms = fetch_once();
    xTimerChangePeriod(timer, pdMS_TO_TICKS(POLL_INTERVAL_MS + backoff_ms), 0);
    while (1) {
        xTaskNotifyWait(0, 0, NULL, portMAX_DELAY);
        backoff_ms = fetch_once();
        xTimerChangePeriod(timer, pdMS_TO_TICKS(POLL_INTERVAL_MS + backoff_ms), 0);
    }
}

static void start_polling(void) {
    if (s_poll_started)
        return;
    s_poll_started = true;
    xTaskCreatePinnedToCore(poll_task, "ci_poll", 10240, NULL, 3, NULL, 0);
}

/* ---- web-dashboard save callback (called from httpd task) ----------------- */

static void on_web_config_save(const settings_t* s) {
    const settings_t* cur = settings_get();
    bool wifi_changed = (strcmp(cur->ssid, s->ssid) != 0 || strcmp(cur->pass, s->pass) != 0);
    lock();
    settings_save(s);
    unlock();

    if (wifi_changed)
        wifi_sta_start(s->ssid, s->pass);

    start_polling();
    ci_app_request_refresh();
}

/* ---- WiFi event callbacks (called from WiFi/IP event task) ---------------- */

static void on_wifi_connected(void) {
    if (!s_web_started) {
        web_server_start(on_web_config_save);
        mdns_init();
        mdns_hostname_set("ci-orb");
        mdns_instance_name_set("CI Status Orb");
        mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);
        s_web_started = true;
    }

    /* Snapshot the actual IP for the screen label. */
    char url[48];
    const char* ip = wifi_sta_get_ip();
    if (ip[0])
        snprintf(url, sizeof(url), "http://%s", ip);
    else
        snprintf(url, sizeof(url), "http://ci-orb.local");

    lock();
    strncpy(s_ip_url, url, sizeof(s_ip_url) - 1);
    s_ip_url[sizeof(s_ip_url) - 1] = '\0';
    s_ip_pending = true;
    unlock();
}

static void on_wifi_disconnected(void) {
    lock();
    s_ip_url[0] = '\0';
    s_ip_pending = true;
    unlock();
}

/* ---- public UI actions (called directly from LVGL_UI event handlers) ------ */

void ci_app_request_refresh(void) {
    if (s_poll_task_handle)
        xTaskNotify(s_poll_task_handle, 0, eNoAction);
}

void ci_app_start_provisioning(void) {
    wifi_sta_start_ap();
    web_server_start(on_web_config_save);
}

void ci_app_open_settings(void) {
    ci_settings_screen();
}

void ci_app_set_branch(const char* branch) {
    const settings_t* cur = settings_get();
    settings_t s = *cur;
    strncpy(s.branch, branch ? branch : "", sizeof(s.branch) - 1);
    s.branch[sizeof(s.branch) - 1] = '\0';
    settings_save(&s);
    ci_app_request_refresh();
}

/* ---- public lifecycle ----------------------------------------------------- */

void ci_app_init(void) {
    s_mux = xSemaphoreCreateMutex();
    assert(s_mux);

    settings_init();

    wifi_sta_set_callbacks(on_wifi_connected, on_wifi_disconnected);

    const settings_t* s = settings_get();
#ifdef DEBUG_WIFI_SSID
    const char* ssid = s->ssid[0] ? s->ssid : DEBUG_WIFI_SSID;
    const char* pass = s->ssid[0] ? s->pass : DEBUG_WIFI_PASS;
    wifi_sta_start(ssid, pass);
    if (settings_is_configured())
        start_polling();
#else
    if (s->ssid[0]) {
        wifi_sta_start(s->ssid, s->pass);
        if (settings_is_configured())
            start_polling();
    } else {
        /* No credentials yet — host the SoftAP so the config form is reachable
         * from a phone on first boot (the STA web server only starts once
         * connected). */
        ci_app_start_provisioning();
    }
#endif
}

void ci_app_create_ui(void) {
    const settings_t* s = settings_get();
    ci_orb();
    ci_set_watch_branch(s->branch);
    if (!s->ssid[0])
        ci_settings_screen();
}

void ci_app_update(void) {
    lock();

    bool ci_pending = s_buf.pending;
    int state = 0;
    char repo[SET_REPO_MAX], title[TITLE_LEN], wf[SET_BRANCH_MAX], info[INFO_LEN];
    if (ci_pending) {
        state = s_buf.state;
        strncpy(repo, s_buf.repo, SET_REPO_MAX);
        strncpy(title, s_buf.title, TITLE_LEN);
        strncpy(wf, s_buf.wf_branch, SET_BRANCH_MAX);
        strncpy(info, s_buf.runinfo, INFO_LEN);
        s_buf.pending = false;
    }

    bool ip_pending = s_ip_pending;
    char ip_url[48] = "";
    if (ip_pending) {
        strncpy(ip_url, s_ip_url, sizeof(ip_url) - 1);
        s_ip_pending = false;
    }

    unlock();

    if (ci_pending)
        update_ci_orb(state, repo, title, wf, info);

    if (ip_pending)
        ci_ui_set_url(ip_url);
}
