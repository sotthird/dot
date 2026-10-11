#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Called with a "US:S|..." line each time the device has fetched fresh plan utilization. */
typedef void (*usage_publish_fn)(const char* line);
/* True while a host has recently sent its own usage lines. */
typedef bool (*usage_host_active_fn)(void);

/* Standalone mode: over Wi-Fi, the device asks api.anthropic.com for the 5-hour and 7-day plan
 * utilization itself, once a minute, with the token given by `dot-host setup`. No host needed.
 * Does nothing unless DOT_USAGE_STANDALONE is on. */
void usage_standalone_start(usage_publish_fn publish, usage_host_active_fn host_active);

typedef enum {
    USAGE_REFRESH_STARTED,     /* a fetch is on its way */
    USAGE_REFRESH_LIMITED,     /* too many taps; *wait_s says when the next one is accepted */
    USAGE_REFRESH_UNAVAILABLE, /* not standalone, no Wi-Fi yet, or a host is feeding the app */
} usage_refresh_t;

/* A tap asks for an immediate refresh. At most three are accepted a minute, and never within five
 * seconds of the last request, so tapping repeatedly cannot hammer the API. Call from the LVGL
 * task. */
usage_refresh_t usage_standalone_refresh(uint32_t* wait_s);
