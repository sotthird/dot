#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "lvgl.h"

#define SPOTIFY_TEXT_LEN 128

typedef struct {
    char track[SPOTIFY_TEXT_LEN];
    char artist[SPOTIFY_TEXT_LEN];
    bool is_playing;
    int32_t progress_ms;
    int32_t duration_ms;
} spotify_track_t;

/* Called with "prev", "play_pause" or "next" when a transport button is tapped. */
typedef void (*spotify_ui_cmd_cb_t)(const char* cmd);

void spotify_ui_create(spotify_ui_cmd_cb_t on_cmd);
void spotify_ui_update(const spotify_track_t* track);

/* Show album art (RGB565, w x h, borrowed: must stay valid) and theme the UI with `color`. */
void spotify_ui_set_art(const uint8_t* data, int w, int h, uint32_t color);
