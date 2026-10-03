#include "app.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "serial_link.h"

#define APP_MAX 8
#define IMAGE_BUF_BYTES (180 * 180 * 2)
#define CMD_DEBOUNCE_MS 500

static const app_t* apps[APP_MAX];
static int app_count;
static SemaphoreHandle_t mutex;
static uint8_t* image_buf;

void app_init(void) {
    mutex = xSemaphoreCreateMutex();
    assert(mutex);

    image_buf = heap_caps_malloc(IMAGE_BUF_BYTES, MALLOC_CAP_SPIRAM);
    assert(image_buf);
}

void app_register(const app_t* app) {
    assert(app_count < APP_MAX);
    apps[app_count++] = app;
}

void app_create_uis(void) {
    for (int i = 0; i < app_count; i++)
        if (apps[i]->create_ui)
            apps[i]->create_ui();
}

void app_update_all(void) {
    for (int i = 0; i < app_count; i++)
        if (apps[i]->update)
            apps[i]->update();
}

void app_handle_line(const char* line) {
    for (int i = 0; i < app_count; i++) {
        if (strncmp(line, apps[i]->prefix, strlen(apps[i]->prefix)) == 0) {
            if (apps[i]->parse)
                apps[i]->parse(line);
            return;
        }
    }
}

void app_handle_image(const uint8_t* data, int w, int h, uint32_t color) {
    for (int i = 0; i < app_count; i++)
        if (apps[i]->image)
            apps[i]->image(data, w, h, color);
}

uint8_t* app_image_buffer(size_t needed) {
    return needed <= IMAGE_BUF_BYTES ? image_buf : NULL;
}

void app_lock(void) {
    xSemaphoreTake(mutex, portMAX_DELAY);
}

void app_unlock(void) {
    xSemaphoreGive(mutex);
}

void app_send_cmd(const char* cmd) {
    static uint32_t last_ms;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_ms < CMD_DEBOUNCE_MS)
        return;
    last_ms = now;

    char msg[32];
    snprintf(msg, sizeof(msg), "CMD:%s\n", cmd);
    serial_link_send(msg);
}
