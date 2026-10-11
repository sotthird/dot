#include "settings.h"

#include <string.h>

#include "nvs.h"

#define NAMESPACE "dot"

static const char* const KEYS[] = {SETTING_WIFI_SSID, SETTING_WIFI_PASS, SETTING_CLAUDE_TOKEN};

static bool known(const char* key) {
    for (size_t i = 0; i < sizeof(KEYS) / sizeof(KEYS[0]); i++)
        if (strcmp(key, KEYS[i]) == 0)
            return true;
    return false;
}

bool settings_get(const char* key, char* out, size_t size, const char* fallback) {
    nvs_handle_t h;
    size_t len = size;
    if (known(key) && nvs_open(NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        esp_err_t err = nvs_get_str(h, key, out, &len);
        nvs_close(h);
        if (err == ESP_OK && out[0] != '\0')
            return true;
    }
    strlcpy(out, fallback ? fallback : "", size);
    return false;
}

bool settings_set(const char* key, const char* value) {
    nvs_handle_t h;
    if (!known(key) || nvs_open(NAMESPACE, NVS_READWRITE, &h) != ESP_OK)
        return false;
    esp_err_t err = value[0] ? nvs_set_str(h, key, value) : nvs_erase_key(h, key);
    if (err == ESP_ERR_NVS_NOT_FOUND)
        err = ESP_OK;
    if (err == ESP_OK)
        err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}
