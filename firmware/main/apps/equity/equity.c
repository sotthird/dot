#include "equity.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "equity_ui.h"

static equity_summary_t summary;
static bool summary_pending;

static struct {
    char value[24];
    char gain[24];
    char currency[8];
    bool pending;
} amounts;

static void create_ui(lv_obj_t* screen) {
    equity_ui_create(screen, app_send_cmd);
}

static equity_state_t state_from_str(const char* s) {
    if (strcmp(s, "ok") == 0)
        return EQUITY_OK;
    if (strcmp(s, "stale") == 0)
        return EQUITY_STALE;
    if (strcmp(s, "auth") == 0)
        return EQUITY_AUTH;
    return EQUITY_OFFLINE;
}

/* EQ:S|<state>|<total %>|<unrealized %>|<positions>|<age seconds> */
static void parse_summary(const char* line) {
    char state[12] = {0};
    equity_summary_t s = {0};
    if (sscanf(line, "EQ:S|%11[^|]|%f|%f|%d|%d", state, &s.total_pct, &s.unrealized_pct,
               &s.positions, &s.age_s) != 5)
        return;
    s.state = state_from_str(state);

    app_lock();
    summary = s;
    summary_pending = true;
    app_unlock();
}

/* EQ:V|<value>|<gain>|<currency> */
static void parse_amounts(const char* line) {
    char value[24] = {0}, gain[24] = {0}, currency[8] = {0};
    if (sscanf(line, "EQ:V|%23[^|]|%23[^|]|%7[^\n]", value, gain, currency) != 3)
        return;

    app_lock();
    memcpy(amounts.value, value, sizeof(value));
    memcpy(amounts.gain, gain, sizeof(gain));
    memcpy(amounts.currency, currency, sizeof(currency));
    amounts.pending = true;
    app_unlock();
}

static void parse(const char* line) {
    if (strncmp(line, "EQ:S|", 5) == 0)
        parse_summary(line);
    else if (strncmp(line, "EQ:V|", 5) == 0)
        parse_amounts(line);
}

static void update(void) {
    app_lock();
    bool has_summary = summary_pending;
    equity_summary_t s = summary;
    summary_pending = false;

    bool has_amounts = amounts.pending;
    char value[24], gain[24], currency[8];
    memcpy(value, amounts.value, sizeof(value));
    memcpy(gain, amounts.gain, sizeof(gain));
    memcpy(currency, amounts.currency, sizeof(currency));
    amounts.pending = false;
    app_unlock();

    if (has_summary)
        equity_ui_update(&s);
    if (has_amounts)
        equity_ui_show_amounts(value, gain, currency);
}

static void suspend(void) {
    equity_ui_suspend();
}

static void resume(void) {
    equity_ui_resume();
}

const app_t equity_app = {
    .name = "equity",
    .prefix = "EQ:",
    .create_ui = create_ui,
    .parse = parse,
    .update = update,
    .suspend = suspend,
    .resume = resume,
};
