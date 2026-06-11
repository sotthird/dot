#pragma once

#include "lvgl.h"

void Lvgl_Example1(void);
void update_cpu_display(float cpu_pct);

typedef void (*spotify_cmd_cb_t)(const char* cmd);

void spotify(void);
void spotify_set_cmd_callback(spotify_cmd_cb_t cb);
void update_spotify(const char* track, const char* artist, bool is_playing, int32_t progress_ms,
                    int32_t duration_ms);
