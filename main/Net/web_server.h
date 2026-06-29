#pragma once

#include "esp_err.h"
#include "settings.h"

/* Called from the httpd task when POST /api/config delivers a validated,
 * fully-merged settings_t. The callee owns saving, reconnecting WiFi, and
 * triggering a CI poll. */
typedef void (*web_config_save_cb_t)(const settings_t* merged);

/* Start the HTTP server on port 80. on_save is invoked on every successful
 * POST /api/config. Idempotent — safe to call again if already running. */
esp_err_t web_server_start(web_config_save_cb_t on_save);

/* Stop the HTTP server. No-op if not running. */
void web_server_stop(void);
