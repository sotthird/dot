#pragma once

#include <stddef.h>
#include <stdint.h>

/* An app is one screen plus the serial messages that drive it. Both halves of the
 * host/firmware contract are keyed by `prefix`: the host sends lines beginning with
 * it, and the app can send "CMD:..." lines back with app_send_cmd(). */
typedef struct {
    const char* prefix;
    /* LVGL task, once at startup: build the screen. */
    void (*create_ui)(void);
    /* Serial task, per received line: decode it and stash the result under app_lock(). */
    void (*parse)(const char* line);
    /* LVGL task, every loop: render whatever parse() or image() stashed. */
    void (*update)(void);
    /* Serial task, optional: a binary image finished arriving in app_image_buffer(). */
    void (*image)(const uint8_t* data, int w, int h, uint32_t color);
} app_t;

void app_init(void);
void app_register(const app_t* app);

void app_create_uis(void);
void app_update_all(void);

/* Called by the serial link. */
void app_handle_line(const char* line);
void app_handle_image(const uint8_t* data, int w, int h, uint32_t color);

/* Buffer an incoming image is received into; NULL if `needed` bytes will not fit. */
uint8_t* app_image_buffer(size_t needed);

/* Guards state shared between parse()/image() and update(). */
void app_lock(void);
void app_unlock(void);

/* Send "CMD:<cmd>\n" to the host. Calls arriving within 500 ms of the last are dropped. */
void app_send_cmd(const char* cmd);
