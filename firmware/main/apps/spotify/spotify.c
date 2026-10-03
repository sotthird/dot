#include "spotify.h"

#include <stdbool.h>
#include <stdio.h>

#include "spotify_ui.h"

static spotify_track_t track;
static bool track_pending;

static struct {
    bool pending;
    int w, h;
    uint32_t color;
} art;

static void create_ui(void) {
    spotify_ui_create(app_send_cmd);
}

static void parse(const char* line) {
    spotify_track_t t = {0};
    int playing = 0;
    long progress = 0, duration = 0;

    if (sscanf(line, "SPOTIFY:%127[^|]|%127[^|]|%d|%ld|%ld", t.track, t.artist, &playing, &progress,
               &duration) < 2)
        return;
    t.is_playing = (playing == 1);
    t.progress_ms = (int32_t)progress;
    t.duration_ms = (int32_t)duration;

    app_lock();
    track = t;
    track_pending = true;
    app_unlock();
}

static void image(const uint8_t* data, int w, int h, uint32_t color) {
    app_lock();
    art.pending = true;
    art.w = w;
    art.h = h;
    art.color = color;
    app_unlock();
}

static void update(void) {
    app_lock();
    bool has_track = track_pending;
    spotify_track_t t = track;
    track_pending = false;

    bool has_art = art.pending;
    int w = art.w, h = art.h;
    uint32_t color = art.color;
    art.pending = false;
    app_unlock();

    if (has_track)
        spotify_ui_update(&t);
    if (has_art)
        spotify_ui_set_art(app_image_buffer(0), w, h, color);
}

const app_t spotify_app = {
    .prefix = "SPOTIFY:",
    .create_ui = create_ui,
    .parse = parse,
    .update = update,
    .image = image,
};
