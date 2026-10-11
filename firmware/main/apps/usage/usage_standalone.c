#include "usage_standalone.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "radio.h"
#include "sdkconfig.h"
#include "settings.h"
#include "usage_limiter.h"

#if CONFIG_DOT_USAGE_STANDALONE

#define TAG "usage"
#define API_URL "https://api.anthropic.com/v1/messages"
#define API_BODY                                                 \
    "{\"model\":\"claude-haiku-4-5-20251001\",\"max_tokens\":1," \
    "\"messages\":[{\"role\":\"user\",\"content\":\"hi\"}]}"
#define POLL_MS 60000
#define RETRY_MS 10000

typedef struct {
    double util_5h, util_7d;
    long reset_5h, reset_7d; /* epoch seconds */
    time_t now;              /* the server's clock, from the Date header; no SNTP needed */
    bool has_5h, has_7d;
} limits_t;

static usage_publish_fn publish;
static usage_host_active_fn host_active;
static TaskHandle_t task;
static limiter_t limiter;
static portMUX_TYPE limiter_lock = portMUX_INITIALIZER_UNLOCKED;

static int64_t now_ms(void) {
    return esp_timer_get_time() / 1000;
}

/* UTC struct tm to epoch seconds (newlib has no timegm). Days-from-civil, after H. Hinnant. */
static time_t utc_seconds(const struct tm* tm) {
    long y = tm->tm_year + 1900L - (tm->tm_mon < 2);
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400;
    long doy = (153 * (tm->tm_mon + (tm->tm_mon < 2 ? 10 : -2)) + 2) / 5 + tm->tm_mday - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    long days = era * 146097 + doe - 719468;
    return days * 86400L + tm->tm_hour * 3600L + tm->tm_min * 60L + tm->tm_sec;
}

static esp_err_t on_http_event(esp_http_client_event_t* e) {
    if (e->event_id != HTTP_EVENT_ON_HEADER)
        return ESP_OK;
    limits_t* l = e->user_data;
    const char* key = e->header_key;
    const char* value = e->header_value;

    if (strcasecmp(key, "anthropic-ratelimit-unified-5h-utilization") == 0) {
        l->util_5h = strtod(value, NULL);
        l->has_5h = true;
    } else if (strcasecmp(key, "anthropic-ratelimit-unified-7d-utilization") == 0) {
        l->util_7d = strtod(value, NULL);
        l->has_7d = true;
    } else if (strcasecmp(key, "anthropic-ratelimit-unified-5h-reset") == 0) {
        l->reset_5h = strtol(value, NULL, 10);
    } else if (strcasecmp(key, "anthropic-ratelimit-unified-7d-reset") == 0) {
        l->reset_7d = strtol(value, NULL, 10);
    } else if (strcasecmp(key, "date") == 0) {
        struct tm tm = {0};
        if (strptime(value, "%a, %d %b %Y %H:%M:%S", &tm))
            l->now = utc_seconds(&tm);
    }
    return ESP_OK;
}

static long minutes_until(long reset, time_t now) {
    if (reset <= 0 || now <= 0)
        return -1;
    long minutes = lround((double)(reset - (long)now) / 60.0);
    return minutes < 0 ? 0 : minutes;
}

/* One 1-token request; the reply's rate-limit headers carry the plan utilization. */
static bool fetch(void) {
    char token[200];
    if (!settings_get(SETTING_CLAUDE_TOKEN, token, sizeof(token), NULL)) {
        ESP_LOGW(TAG, "no Claude token on the device; run `dot-host setup`");
        return false;
    }
    char bearer[sizeof(token) + 8];
    snprintf(bearer, sizeof(bearer), "Bearer %s", token);
    limits_t limits = {0};
    esp_http_client_config_t cfg = {
        .url = API_URL,
        .method = HTTP_METHOD_POST,
        .event_handler = on_http_event,
        .user_data = &limits,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 20000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
        return false;
    esp_http_client_set_header(client, "Authorization", bearer);
    esp_http_client_set_header(client, "anthropic-version", "2023-06-01");
    esp_http_client_set_header(client, "anthropic-beta", "oauth-2025-04-20");
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_header(client, "User-Agent", "claude-code/2.1.5");
    esp_http_client_set_post_field(client, API_BODY, strlen(API_BODY));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "request failed: %s", esp_err_to_name(err));
        return false;
    }
    if (status == 401 || status == 403) {
        ESP_LOGW(TAG,
                 "HTTP %d: the token is invalid or expired; make a new one with "
                 "`claude setup-token`",
                 status);
        return false;
    }
    if (!limits.has_5h || !limits.has_7d) {
        ESP_LOGW(TAG, "HTTP %d without 5h/7d rate-limit headers", status);
        return false;
    }

    char line[64];
    snprintf(line, sizeof(line), "US:S|--|--|--|--|%ld|%ld|%ld|%ld", lround(limits.util_5h * 100),
             lround(limits.util_7d * 100), minutes_until(limits.reset_5h, limits.now),
             minutes_until(limits.reset_7d, limits.now));
    publish(line);
    return true;
}

static void usage_task(void* arg) {
    uint32_t wait_ms = RETRY_MS;
    for (;;) {
        /* Sleep until the next automatic refresh, or until a tap wakes us early. */
        ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(wait_ms));

        if (radio_wifi_state() != RADIO_CONNECTED) {
            wait_ms = RETRY_MS;
            continue;
        }
        /* While a host is feeding the app it has the token counts too: stay quiet. */
        bool ok = true;
        if (!host_active()) {
            portENTER_CRITICAL(&limiter_lock);
            limiter_note_request(&limiter, now_ms());
            portEXIT_CRITICAL(&limiter_lock);
            ok = fetch();
        }
        wait_ms = ok ? POLL_MS : RETRY_MS;
    }
}

usage_refresh_t usage_standalone_refresh(uint32_t* wait_s) {
    if (!task || radio_wifi_state() != RADIO_CONNECTED || host_active())
        return USAGE_REFRESH_UNAVAILABLE;

    int64_t wait = 0;
    portENTER_CRITICAL(&limiter_lock);
    bool allowed = limiter_try(&limiter, now_ms(), &wait);
    portEXIT_CRITICAL(&limiter_lock);
    if (!allowed) {
        *wait_s = (uint32_t)((wait + 999) / 1000);
        return USAGE_REFRESH_LIMITED;
    }
    xTaskNotifyGive(task);
    return USAGE_REFRESH_STARTED;
}

void usage_standalone_start(usage_publish_fn publish_fn, usage_host_active_fn host_active_fn) {
    publish = publish_fn;
    host_active = host_active_fn;
    xTaskCreate(usage_task, "usage", 8192, NULL, 3, &task);
}

#else

void usage_standalone_start(usage_publish_fn publish_fn, usage_host_active_fn host_active_fn) {
    (void)publish_fn;
    (void)host_active_fn;
}

usage_refresh_t usage_standalone_refresh(uint32_t* wait_s) {
    (void)wait_s;
    return USAGE_REFRESH_UNAVAILABLE;
}

#endif
