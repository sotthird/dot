#include "cpu_app.h"

#include <stdbool.h>
#include <stdio.h>

#include "LVGL_Example.h"
#include "app.h"

typedef struct {
    float cpu_pct;
    bool pending;
} cpu_buf_t;

static cpu_buf_t buf = {0};

static void create_ui(void) {
    Lvgl_Example1();
}

static void parse(const char* line) {
    float val;
    if (sscanf(line, "CPU:%f", &val) != 1)
        return;

    app_lock();
    buf.cpu_pct = val;
    buf.pending = true;
    app_unlock();
}

static void update(void) {
    app_lock();
    if (!buf.pending) {
        app_unlock();
        return;
    }
    float val = buf.cpu_pct;
    buf.pending = false;
    app_unlock();

    update_cpu_display(val);
}

const app_t cpu_app = {
    .prefix = "CPU:",
    .init = NULL,
    .create_ui = create_ui,
    .parse = parse,
    .update = update,
};
