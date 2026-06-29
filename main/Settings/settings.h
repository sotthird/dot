#pragma once

#include <stdbool.h>

#include "esp_err.h"

/* Max field lengths (including the NUL terminator). */
#define SET_SSID_MAX 33   /* 802.11 SSID is at most 32 bytes */
#define SET_PASS_MAX 65   /* WPA2 passphrase is at most 63 bytes */
#define SET_TOKEN_MAX 256 /* GitHub PAT (classic or fine-grained) */
#define SET_REPO_MAX 80   /* "owner/name" */
#define SET_BRANCH_MAX 64 /* git branch name */

typedef struct {
    char ssid[SET_SSID_MAX];
    char pass[SET_PASS_MAX];
    char token[SET_TOKEN_MAX];
    char repo[SET_REPO_MAX];
    char branch[SET_BRANCH_MAX];
} settings_t;

/* Bring up NVS (encrypted when CONFIG_NVS_ENCRYPTION is set) and load the saved
 * config into the in-memory cache. Safe to call once at startup. */
esp_err_t settings_init(void);

/* The current cached config. Never NULL after settings_init(). */
const settings_t* settings_get(void);

/* Persist a new config to NVS and update the cache. */
esp_err_t settings_save(const settings_t* s);

/* True once the minimum needed to run is present (WiFi SSID + token + repo). */
bool settings_is_configured(void);
