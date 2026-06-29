#include "settings.h"

#include <string.h>

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define TAG "settings"
#define NS "ci_cfg"

static settings_t cache = {0};

/* Load one string key into dst (dst stays unchanged / empty on miss). */
static void load_str(nvs_handle_t h, const char* key, char* dst, size_t cap) {
    size_t len = cap;
    if (nvs_get_str(h, key, dst, &len) != ESP_OK)
        dst[0] = '\0';
}

esp_err_t settings_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    strncpy(cache.branch, "main", sizeof(cache.branch) - 1);

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        load_str(h, "ssid", cache.ssid, sizeof(cache.ssid));
        load_str(h, "pass", cache.pass, sizeof(cache.pass));
        load_str(h, "token", cache.token, sizeof(cache.token));
        load_str(h, "repo", cache.repo, sizeof(cache.repo));
        load_str(h, "branch", cache.branch, sizeof(cache.branch));
        nvs_close(h);
    } else {
        ESP_LOGW(TAG, "no saved config yet");
    }
    return ESP_OK;
}

const settings_t* settings_get(void) {
    return &cache;
}

esp_err_t settings_save(const settings_t* s) {
    if (!s)
        return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t ret = nvs_open(NS, NVS_READWRITE, &h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed: %s", esp_err_to_name(ret));
        return ret;
    }

    nvs_set_str(h, "ssid", s->ssid);
    nvs_set_str(h, "pass", s->pass);
    nvs_set_str(h, "token", s->token);
    nvs_set_str(h, "repo", s->repo);
    nvs_set_str(h, "branch", s->branch);

    ret = nvs_commit(h);
    nvs_close(h);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return ret;
    }

    cache = *s;
    return ESP_OK;
}

bool settings_is_configured(void) {
    return cache.ssid[0] && cache.token[0] && cache.repo[0];
}
