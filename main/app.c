#include "app.h"

#include <assert.h>
#include <string.h>

#include "driver/usb_serial_jtag.h"
#include "esp_heap_caps.h"

#define IMG_BUF_MAX (180 * 180 * 2)

static const app_t* apps[APP_MAX];
static int app_count = 0;
static SemaphoreHandle_t app_mux = NULL;
static uint8_t* img_buf = NULL;

void app_registry_init(void) {
    app_mux = xSemaphoreCreateMutex();
    assert(app_mux);

    img_buf = heap_caps_malloc(IMG_BUF_MAX, MALLOC_CAP_SPIRAM);
    assert(img_buf);
}

void app_register(const app_t* app) {
    if (app_count < APP_MAX)
        apps[app_count++] = app;
}

void app_start_all(void) {
    for (int i = 0; i < app_count; i++) {
        if (apps[i]->init)
            apps[i]->init();
        if (apps[i]->create_ui)
            apps[i]->create_ui();
    }
}

void app_handle_line(const char* line) {
    for (int i = 0; i < app_count; i++) {
        if (strncmp(line, apps[i]->prefix, strlen(apps[i]->prefix)) == 0) {
            if (apps[i]->parse)
                apps[i]->parse(line);
            break;
        }
    }
}

void app_handle_image(const uint8_t* data, int w, int h, uint32_t color) {
    for (int i = 0; i < app_count; i++)
        if (apps[i]->image)
            apps[i]->image(data, w, h, color);
}

void app_update_all(void) {
    for (int i = 0; i < app_count; i++)
        if (apps[i]->update)
            apps[i]->update();
}

uint8_t* app_get_image_buffer(size_t needed) {
    if (needed > IMG_BUF_MAX)
        return NULL;
    return img_buf;
}

void app_lock(void) {
    xSemaphoreTake(app_mux, portMAX_DELAY);
}
void app_unlock(void) {
    xSemaphoreGive(app_mux);
}

void app_send(const char* msg) {
    usb_serial_jtag_write_bytes((const uint8_t*)msg, strlen(msg), pdMS_TO_TICKS(100));
}
