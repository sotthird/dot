#include "cpu.h"

#include <stdbool.h>
#include <stdio.h>

#include "cpu_ui.h"

static struct {
    float cpu_pct;
    bool pending;
} latest;

static void parse(const char* line) {
    float pct;
    if (sscanf(line, "CPU:%f", &pct) != 1)
        return;

    app_lock();
    latest.cpu_pct = pct;
    latest.pending = true;
    app_unlock();
}

static void update(void) {
    app_lock();
    bool pending = latest.pending;
    float pct = latest.cpu_pct;
    latest.pending = false;
    app_unlock();

    if (pending)
        cpu_ui_update(pct);
}

const app_t cpu_app = {
    .name = "cpu",
    .prefix = "CPU:",
    .create_ui = cpu_ui_create,
    .parse = parse,
    .update = update,
};
