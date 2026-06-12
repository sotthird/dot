#pragma once

#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define APP_MAX 8

typedef void (*app_fn)(void);
typedef void (*app_parse_fn)(const char* line);
typedef void (*app_image_fn)(const uint8_t* data, int w, int h, uint32_t color);

typedef struct {
    const char* prefix;
    app_fn init;
    app_fn create_ui;
    app_parse_fn parse;
    app_fn update;
    app_image_fn image;
} app_t;

void app_registry_init(void);
void app_register(const app_t* app);

void app_start_all(void);
void app_handle_line(const char* line);
void app_handle_image(const uint8_t* data, int w, int h, uint32_t color);
void app_update_all(void);

uint8_t* app_get_image_buffer(size_t needed);

void app_lock(void);
void app_unlock(void);

void app_send(const char* msg);
