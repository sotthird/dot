#pragma once

#include <stddef.h>
#include <stdint.h>

#include "lvgl.h"

/* An app is one screen plus the serial messages that drive it. Both halves of the
 * host/firmware contract are keyed by `prefix`: the host sends lines beginning with
 * it, and the app can send "CMD:..." lines back with app_send_cmd().
 *
 * Apps are registered in swipe order and only the one on screen is active: the others are on
 * standby, receiving no messages and no update() calls. */
typedef struct {
    /* The host's name for this app, as in `dot-host <name>`. */
    const char* name;
    const char* prefix;
    /* LVGL task, once at startup: build the UI on `screen`, which the framework owns. */
    void (*create_ui)(lv_obj_t* screen);
    /* Serial task, per received line: decode it and stash the result under app_lock(). */
    void (*parse)(const char* line);
    /* LVGL task, every loop: render whatever parse() or image() stashed. */
    void (*update)(void);
    /* Serial task, optional: a binary image finished arriving in app_image_buffer(). */
    void (*image)(const uint8_t* data, int w, int h, uint32_t color);
    /* LVGL task, optional: the app is going on standby / coming back on screen. Stop and restart
     * whatever keeps running by itself, such as timers and animations. */
    void (*suspend)(void);
    void (*resume)(void);
} app_t;

void app_init(void);
void app_register(const app_t* app);

/* Build every app's screen and show the first one. */
void app_create_uis(void);
/* Run the active app's update(). */
void app_update_active(void);

/* Called by the serial link. Lines and images go to the active app only; the rest are dropped. */
void app_handle_line(const char* line);
void app_handle_image(const uint8_t* data, int w, int h, uint32_t color);

/* Buffer an incoming image is received into; NULL if `needed` bytes will not fit. */
uint8_t* app_image_buffer(size_t needed);

/* Guards state shared between parse()/image() and update(). */
void app_lock(void);
void app_unlock(void);

/* Send "CMD:<cmd>\n" to the host. Calls arriving within 500 ms of the last are dropped. */
void app_send_cmd(const char* cmd);
