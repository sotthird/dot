#include "usage.h"

#include <stdbool.h>
#include <string.h>

#include "esp_timer.h"
#include "usage_standalone.h"
#include "usage_ui.h"

#define FIELD_MAX 12
#define HOST_QUIET_US (120LL * 1000000) /* standalone fetching resumes after this long */

static volatile int64_t last_host_us = -HOST_QUIET_US;

static struct {
    char today[FIELD_MAX];
    char five_hours[FIELD_MAX];
    char week[FIELD_MAX];
    char messages[FIELD_MAX];
    char pct_5h[FIELD_MAX];
    char pct_7d[FIELD_MAX];
    char reset_5h[FIELD_MAX];
    char reset_7d[FIELD_MAX];
    bool pending;
} latest;

/* Copy the next '|'-separated field of `*p` into `dst`, truncating, and advance `*p` past it.
 * Returns false if there is no field left. */
static bool next_field(const char** p, char* dst) {
    if (!*p)
        return false;
    const char* end = strchr(*p, '|');
    size_t len = end ? (size_t)(end - *p) : strcspn(*p, "\r\n");
    if (len >= FIELD_MAX)
        len = FIELD_MAX - 1;
    memcpy(dst, *p, len);
    dst[len] = '\0';
    *p = end ? end + 1 : NULL;
    return true;
}

/* US:S|<today>|<5h>|<7d>|<messages today>|<5h %>|<7d %>|<5h reset min>|<7d reset min>
 * A percentage or reset time of -1 (or a missing field) means the host does not know it. */
static void ingest(const char* line) {
    if (strncmp(line, "US:S|", 5) != 0)
        return;

    char today[FIELD_MAX], five_hours[FIELD_MAX], week[FIELD_MAX], messages[FIELD_MAX];
    char pct_5h[FIELD_MAX] = "-1", pct_7d[FIELD_MAX] = "-1";
    char reset_5h[FIELD_MAX] = "-1", reset_7d[FIELD_MAX] = "-1";
    const char* p = line + 5;
    if (!next_field(&p, today) || !next_field(&p, five_hours) || !next_field(&p, week) ||
        !next_field(&p, messages))
        return;
    if (next_field(&p, pct_5h) && next_field(&p, pct_7d) && next_field(&p, reset_5h))
        next_field(&p, reset_7d);

    app_lock();
    memcpy(latest.today, today, FIELD_MAX);
    memcpy(latest.five_hours, five_hours, FIELD_MAX);
    memcpy(latest.week, week, FIELD_MAX);
    memcpy(latest.messages, messages, FIELD_MAX);
    memcpy(latest.pct_5h, pct_5h, FIELD_MAX);
    memcpy(latest.pct_7d, pct_7d, FIELD_MAX);
    memcpy(latest.reset_5h, reset_5h, FIELD_MAX);
    memcpy(latest.reset_7d, reset_7d, FIELD_MAX);
    latest.pending = true;
    app_unlock();
}

static void parse(const char* line) {
    last_host_us = esp_timer_get_time();
    ingest(line);
}

static bool host_active(void) {
    return esp_timer_get_time() - last_host_us < HOST_QUIET_US;
}

static void create_ui(lv_obj_t* screen) {
    usage_ui_create(screen);
    usage_standalone_start(ingest, host_active);
}

static void update(void) {
    app_lock();
    bool pending = latest.pending;
    typeof(latest) copy = latest;
    latest.pending = false;
    app_unlock();

    if (pending)
        usage_ui_update(copy.today, copy.five_hours, copy.week, copy.messages, copy.pct_5h,
                        copy.pct_7d, copy.reset_5h, copy.reset_7d);
}

const app_t usage_app = {
    .name = "usage",
    .prefix = "US:",
    .create_ui = create_ui,
    .parse = parse,
    .update = update,
};
