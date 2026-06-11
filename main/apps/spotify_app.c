#include "spotify_app.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "LVGL_Example.h"
#include "app.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TEXT_LEN 128
#define CMD_DEBOUNCE_MS 500

typedef struct {
    char track[TEXT_LEN];
    char artist[TEXT_LEN];
    bool is_playing;
    int32_t progress_ms;
    int32_t duration_ms;
    bool pending;
} spotify_buf_t;

static spotify_buf_t buf = {0};

static void send_cmd(const char* cmd) {
    static uint32_t last_ms = 0;
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    if (now - last_ms < CMD_DEBOUNCE_MS)
        return;
    last_ms = now;

    char msg[32];
    snprintf(msg, sizeof(msg), "CMD:%s\n", cmd);
    app_send(msg);
}

static void init(void) {
    spotify_set_cmd_callback(send_cmd);
}

static void create_ui(void) {
    spotify();
}

static void parse(const char* line) {
    char track[TEXT_LEN], artist[TEXT_LEN];
    int playing = 0;
    long prog = 0, dur = 0;

    if (sscanf(line, "SPOTIFY:%127[^|]|%127[^|]|%d|%ld|%ld", track, artist, &playing, &prog, &dur) <
        2)
        return;

    app_lock();
    strncpy(buf.track, track, TEXT_LEN - 1);
    strncpy(buf.artist, artist, TEXT_LEN - 1);
    buf.is_playing = (playing == 1);
    buf.progress_ms = (int32_t)prog;
    buf.duration_ms = (int32_t)dur;
    buf.pending = true;
    app_unlock();
}

static void update(void) {
    app_lock();
    if (!buf.pending) {
        app_unlock();
        return;
    }
    spotify_buf_t local = buf;
    buf.pending = false;
    app_unlock();

    update_spotify(local.track, local.artist, local.is_playing, local.progress_ms,
                   local.duration_ms);
}

const app_t spotify_app = {
    .prefix = "SPOTIFY:",
    .init = init,
    .create_ui = create_ui,
    .parse = parse,
    .update = update,
};
