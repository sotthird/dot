#pragma once

#include <stdbool.h>
#include <stdint.h>

/* How often a tap may trigger a refresh: at most LIMITER_MAX_PER_WINDOW within any
 * LIMITER_WINDOW_MS, and never closer than LIMITER_MIN_GAP_MS to the last request of any kind
 * (the automatic once-a-minute one included). Pure functions of the clock, so they are easy to
 * test; the caller owns the state. */
#define LIMITER_MAX_PER_WINDOW 3
#define LIMITER_WINDOW_MS 60000
#define LIMITER_MIN_GAP_MS 5000

typedef struct {
    int64_t taken_ms[LIMITER_MAX_PER_WINDOW]; /* when each recent manual refresh was allowed */
    int next;                                 /* slot holding the oldest of them */
    int64_t last_any_ms;                      /* last request of any kind, for the minimum gap */
} limiter_t;

/* Mark a request that was not a tap (the automatic refresh). */
static inline void limiter_note_request(limiter_t* l, int64_t now_ms) {
    l->last_any_ms = now_ms;
}

/* Ask to refresh now. True and counted if allowed; otherwise false, with the milliseconds to wait
 * before it would be allowed in *wait_ms. */
static inline bool limiter_try(limiter_t* l, int64_t now_ms, int64_t* wait_ms) {
    int64_t wait = 0;
    int64_t gap_ends = l->last_any_ms + LIMITER_MIN_GAP_MS;
    if (l->last_any_ms > 0 && now_ms < gap_ends)
        wait = gap_ends - now_ms;
    int64_t window_ends = l->taken_ms[l->next] + LIMITER_WINDOW_MS;
    if (l->taken_ms[l->next] > 0 && now_ms < window_ends && window_ends - now_ms > wait)
        wait = window_ends - now_ms;
    if (wait > 0) {
        *wait_ms = wait;
        return false;
    }
    l->taken_ms[l->next] = now_ms;
    l->next = (l->next + 1) % LIMITER_MAX_PER_WINDOW;
    l->last_any_ms = now_ms;
    return true;
}
