#pragma once

#include <stdbool.h>

#include "lvgl.h"

typedef enum {
    EQUITY_OK,
    EQUITY_STALE,   /* connected, but prices are old or some are missing */
    EQUITY_OFFLINE, /* the host cannot reach EquityWatch */
    EQUITY_AUTH,    /* EquityWatch refused the host's login */
} equity_state_t;

typedef struct {
    equity_state_t state;
    float total_pct;      /* total return on cost */
    float unrealized_pct; /* return on what is still held */
    int positions;
    int age_s; /* age of the oldest price, -1 if unknown */
} equity_summary_t;

#define EQUITY_TOP_COUNT 3

/* The best performing holdings, best first. Shown under the figures while the screen is idle. */
typedef struct {
    int count; /* 0 to EQUITY_TOP_COUNT */
    char symbol[EQUITY_TOP_COUNT][12];
    float pct[EQUITY_TOP_COUNT];
} equity_top_t;

/* Called with "eq_reveal" when the screen is tapped. */
typedef void (*equity_ui_cmd_cb_t)(const char* cmd);

/* Portfolio return in big type; tapping shows the amounts for a few seconds. */
void equity_ui_create(lv_obj_t* screen, equity_ui_cmd_cb_t on_cmd);
void equity_ui_update(const equity_summary_t* summary);

void equity_ui_set_top(const equity_top_t* top);

/* Standby: the screen goes back to its normal look, and idle detection stops until resumed. */
void equity_ui_suspend(void);
void equity_ui_resume(void);

/* Show the value and gain (already formatted text) for a few seconds. */
void equity_ui_show_amounts(const char* value, const char* gain, const char* currency);
