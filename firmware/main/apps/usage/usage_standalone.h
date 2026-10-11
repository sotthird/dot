#pragma once

#include <stdbool.h>

/* Called with a "US:S|..." line each time the device has fetched fresh plan utilization. */
typedef void (*usage_publish_fn)(const char* line);
/* True while a host has recently sent its own usage lines. */
typedef bool (*usage_host_active_fn)(void);

/* Standalone mode: over Wi-Fi, the device asks api.anthropic.com for the 5-hour and 7-day plan
 * utilization itself, once a minute, with the token set in menuconfig. No host needed. Does
 * nothing unless DOT_USAGE_STANDALONE is on and a token is set. */
void usage_standalone_start(usage_publish_fn publish, usage_host_active_fn host_active);
