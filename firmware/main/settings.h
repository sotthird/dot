#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Settings the user gives the device over USB (`dot-host setup`) instead of building them in:
 * Wi-Fi credentials and the Claude token. Kept in flash (NVS), not in the firmware image. */
#define SETTING_WIFI_SSID "wifi_ssid"
#define SETTING_WIFI_PASS "wifi_pass"
#define SETTING_CLAUDE_TOKEN "claude_tok"

/* Copy the stored value into `out`, else `fallback` (may be NULL for ""). True if one is stored. */
bool settings_get(const char* key, char* out, size_t size, const char* fallback);

/* Store `value`; an empty value forgets the setting. False for an unknown key or a flash error. */
bool settings_set(const char* key, const char* value);
