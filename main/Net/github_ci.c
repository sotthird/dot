#include "github_ci.h"

#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define TAG "github_ci"
#define RESP_CAP 24576 /* one run with per_page=1 fits comfortably */

/* GitHub Actions status/conclusion → orb state, mirroring the old host logic. */
static gh_state_t map_state(const char* status, const char* conclusion) {
    if (!status)
        return GH_UNKNOWN;
    if (!strcmp(status, "queued") || !strcmp(status, "in_progress") ||
        !strcmp(status, "requested") || !strcmp(status, "waiting") || !strcmp(status, "pending"))
        return GH_RUNNING;
    if (!strcmp(status, "completed") && conclusion) {
        if (!strcmp(conclusion, "success"))
            return GH_SUCCESS;
        if (!strcmp(conclusion, "failure") || !strcmp(conclusion, "timed_out") ||
            !strcmp(conclusion, "startup_failure"))
            return GH_FAILURE;
    }
    return GH_UNKNOWN;
}

/* Copy src into dst keeping only printable ASCII (device fonts are ASCII-only)
 * and dropping the '|' field delimiter, NUL-terminated within cap. */
static void ascii_copy(char* dst, size_t cap, const char* src) {
    size_t j = 0;
    if (src) {
        for (size_t i = 0; src[i] && j < cap - 1; i++) {
            unsigned char c = (unsigned char)src[i];
            if (c >= 0x20 && c < 0x7F && c != '|')
                dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

static const char* cj_str(const cJSON* obj, const char* key) {
    const cJSON* item = cJSON_GetObjectItemCaseSensitive(obj, key);
    return cJSON_IsString(item) ? item->valuestring : NULL;
}

static void parse_runs(const char* body, const char* repo, gh_result_t* out) {
    cJSON* root = cJSON_Parse(body);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return;
    }

    const cJSON* runs = cJSON_GetObjectItemCaseSensitive(root, "workflow_runs");
    const cJSON* run = cJSON_IsArray(runs) ? cJSON_GetArrayItem(runs, 0) : NULL;

    ascii_copy(out->repo, sizeof(out->repo), repo);

    if (!run) {
        out->state = GH_UNKNOWN;
        ascii_copy(out->wf_branch, sizeof(out->wf_branch), "No runs");
        out->title[0] = '\0';
        out->runinfo[0] = '\0';
        out->ok = true;
        cJSON_Delete(root);
        return;
    }

    out->state = map_state(cj_str(run, "status"), cj_str(run, "conclusion"));

    const char* title = cj_str(run, "display_title");
    if (!title)
        title = cj_str(run, "name");
    ascii_copy(out->title, sizeof(out->title), title);

    char wf[96];
    snprintf(wf, sizeof(wf), "%s / %s", cj_str(run, "name") ? cj_str(run, "name") : "CI",
             cj_str(run, "head_branch") ? cj_str(run, "head_branch") : "");
    ascii_copy(out->wf_branch, sizeof(out->wf_branch), wf);

    const cJSON* number = cJSON_GetObjectItemCaseSensitive(run, "run_number");
    int num = cJSON_IsNumber(number) ? number->valueint : 0;
    const char* word = out->state == GH_RUNNING   ? "running"
                       : out->state == GH_SUCCESS ? "passed"
                       : out->state == GH_FAILURE ? "failed"
                                                  : "";
    if (num)
        snprintf(out->runinfo, sizeof(out->runinfo), "#%d %s", num, word);
    else
        ascii_copy(out->runinfo, sizeof(out->runinfo), word);

    out->ok = true;
    cJSON_Delete(root);
}

bool github_ci_fetch(const char* token, const char* repo, const char* branch, gh_result_t* out) {
    memset(out, 0, sizeof(*out));
    if (!token || !token[0] || !repo || !repo[0])
        return false;

    char url[256];
    if (branch && branch[0])
        snprintf(url, sizeof(url),
                 "https://api.github.com/repos/%s/actions/runs?per_page=1&branch=%s", repo, branch);
    else
        snprintf(url, sizeof(url), "https://api.github.com/repos/%s/actions/runs?per_page=1", repo);

    esp_http_client_config_t cfg = {
        .url = url,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
        .buffer_size = 2048,
        .buffer_size_tx = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client)
        return false;

    char auth[288]; /* "Bearer " + token (<=255) + NUL */
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    esp_http_client_set_header(client, "Authorization", auth);
    esp_http_client_set_header(client, "Accept", "application/vnd.github+json");
    esp_http_client_set_header(client, "User-Agent", "dot-ci-orb");
    esp_http_client_set_header(client, "X-GitHub-Api-Version", "2022-11-28");

    char* body = heap_caps_malloc(RESP_CAP, MALLOC_CAP_SPIRAM);
    if (!body) {
        esp_http_client_cleanup(client);
        return false;
    }

    bool ok = false;
    esp_err_t err = esp_http_client_open(client, 0);
    if (err == ESP_OK) {
        esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        int total = 0, r;
        while ((r = esp_http_client_read(client, body + total, RESP_CAP - 1 - total)) > 0) {
            total += r;
            if (total >= RESP_CAP - 1)
                break;
        }
        body[total] = '\0';

        if (status == 200) {
            parse_runs(body, repo, out);
            ok = out->ok;
        } else {
            /* On 429 (secondary rate limit) or 403, respect Retry-After.
             * GitHub sends it as seconds-to-wait on 429; fall back to 60 s
             * for a 403 with no header (primary rate-limit window). */
            uint32_t backoff = 0;
            if (status == 429 || status == 403) {
                char* hdr = NULL;
                if (esp_http_client_get_header(client, "Retry-After", &hdr) == ESP_OK && hdr) {
                    long v = strtol(hdr, NULL, 10);
                    backoff = (v > 0) ? (uint32_t)v : 60;
                } else {
                    backoff = 60;
                }
                /* Cap to avoid sleeping indefinitely on a bad response. */
                if (backoff > 300)
                    backoff = 300;
                out->retry_after_s = backoff;
                ESP_LOGW(TAG, "HTTP %d — backing off %lu s", status, (unsigned long)backoff);
            } else {
                ESP_LOGW(TAG, "HTTP %d for %s", status, url);
            }
        }
    } else {
        ESP_LOGW(TAG, "open failed: %s", esp_err_to_name(err));
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    heap_caps_free(body);
    return ok;
}
