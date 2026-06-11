#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define APP_MAX 8

typedef void (*app_fn)(void);
typedef void (*app_parse_fn)(const char* line);

typedef struct {
    const char* prefix;
    app_fn init;
    app_fn create_ui;
    app_parse_fn parse;
    app_fn update;
} app_t;

void app_registry_init(void);
void app_register(const app_t* app);

void app_start_all(void);
void app_handle_line(const char* line);
void app_update_all(void);

void app_lock(void);
void app_unlock(void);

void app_send(const char* msg);
