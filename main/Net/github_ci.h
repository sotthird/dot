#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    GH_UNKNOWN = 0,
    GH_RUNNING,
    GH_SUCCESS,
    GH_FAILURE,
} gh_state_t;

typedef struct {
    gh_state_t state;
    char repo[80];          /* "owner/name" (echoed from the request) */
    char title[96];         /* run display title */
    char wf_branch[64];     /* "<workflow> / <branch>" */
    char runinfo[32];       /* e.g. "#129 running" */
    bool ok;                /* false on network/HTTP/parse error */
    uint32_t retry_after_s; /* seconds to back off (from Retry-After header); 0 = none */
} gh_result_t;

/* Fetch the latest Actions run for repo (and branch, if non-empty) and fill
 * `out`. Returns out->ok. Blocking; call from a task, not the LVGL thread. */
bool github_ci_fetch(const char* token, const char* repo, const char* branch, gh_result_t* out);
