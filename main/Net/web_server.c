#include "web_server.h"

#include <string.h>

#include "cJSON.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "settings.h"

#define TAG "web_srv"

static httpd_handle_t s_server = NULL;
static web_config_save_cb_t s_on_save = NULL;

/* Lightweight single-page dashboard — single-quoted HTML attributes to avoid
 * C-string escaping.  All JS uses single-quoted strings for the same reason. */
static const char DASHBOARD_HTML[] =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CI Orb</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:480px;margin:2rem auto;padding:0 1rem;"
    "background:#1a1a1a;color:#e0e0e0}"
    "h1{font-size:1.4rem;color:#7fb3e8;margin-bottom:.25rem}"
    "p{margin:0 0 1rem;font-size:.8rem;color:#666}"
    "label{display:block;margin-top:1rem;font-size:.85rem;color:#888}"
    "input{width:100%;box-sizing:border-box;padding:.5rem;background:#2a2a2a;"
    "border:1px solid #444;color:#e0e0e0;border-radius:4px;font-size:1rem}"
    ".pw-wrap{position:relative}"
    ".pw-wrap input{padding-right:3.2rem}"
    ".toggle{position:absolute;right:.5rem;top:.4rem;background:none;border:none;"
    "color:#888;cursor:pointer;font-size:.8rem;padding:.15rem .3rem}"
    "button.save{margin-top:1.5rem;width:100%;padding:.75rem;background:#2e7d32;"
    "color:#fff;border:none;border-radius:4px;font-size:1rem;cursor:pointer}"
    "button.save:active{background:#1b5e20}"
    "#msg{margin-top:1rem;padding:.6rem;border-radius:4px;display:none}"
    ".ok{background:#1b5e20;color:#a5d6a7}.err{background:#4e0000;color:#ef9a9a}"
    "</style></head><body>"
    "<h1>CI Orb</h1><p>Configure device settings</p>"
    "<form id='f'>"
    "<label>WiFi SSID</label><input id='ssid' name='ssid'>"
    "<label>WiFi Password</label>"
    "<div class='pw-wrap'>"
    "<input id='pass' name='pass' type='password'>"
    "<button type='button' class='toggle' data-for='pass'>Show</button>"
    "</div>"
    "<label>GitHub Token</label>"
    "<div class='pw-wrap'>"
    "<input id='token' name='token' type='password'>"
    "<button type='button' class='toggle' data-for='token'>Show</button>"
    "</div>"
    "<label>Repository (owner/name)</label>"
    "<input id='repo' name='repo' placeholder='owner/repo'>"
    "<label>Branch (blank = all branches)</label>"
    "<input id='branch' name='branch' placeholder='main'>"
    "<button class='save' type='submit'>Save &amp; Apply</button>"
    "</form>"
    "<div id='msg'></div>"
    "<script>"
    "document.querySelectorAll('.toggle').forEach(function(b){"
    "b.onclick=function(){"
    "var i=document.getElementById(b.dataset.for);"
    "i.type=i.type==='password'?'text':'password';"
    "};"
    "});"
    "fetch('/api/config').then(function(r){return r.json();}).then(function(d){"
    "['ssid','pass','token','repo','branch'].forEach(function(k){"
    "if(d[k])document.getElementById(k).value=d[k];"
    "});"
    "});"
    "document.getElementById('f').onsubmit=function(e){"
    "e.preventDefault();"
    "var o={};"
    "['ssid','pass','token','repo','branch'].forEach(function(k){"
    "o[k]=document.getElementById(k).value;"
    "});"
    "fetch('/api/config',{"
    "method:'POST',"
    "headers:{'Content-Type':'application/json'},"
    "body:JSON.stringify(o)"
    "}).then(function(r){return r.json();}).then(function(d){"
    "var m=document.getElementById('msg');"
    "m.style.display='block';"
    "if(d.ok){"
    "m.className='ok';"
    "m.textContent='Saved! Device will update shortly.';"
    "}else{"
    "m.className='err';"
    "m.textContent='Error: '+(d.error||'unknown');"
    "}"
    "}).catch(function(){"
    "var m=document.getElementById('msg');"
    "m.style.display='block';"
    "m.className='err';"
    "m.textContent='Network error';"
    "});"
    "};"
    "</script></body></html>";

/* Overwrite dst from a JSON string field, skipping absent fields and the
 * sentinel "***" (which means "keep existing value"). */
static void apply_field(const cJSON* root, const char* key, char* dst, size_t max) {
    cJSON* item = cJSON_GetObjectItemCaseSensitive(root, key);
    if (!item || !cJSON_IsString(item))
        return;
    const char* val = item->valuestring;
    if (val && strcmp(val, "***") == 0)
        return;
    strncpy(dst, val ? val : "", max - 1);
    dst[max - 1] = '\0';
}

static esp_err_t get_root_handler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_config_handler(httpd_req_t* req) {
    const settings_t* s = settings_get();
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"ssid\":\"%s\",\"pass\":\"%s\",\"token\":\"%s\","
             "\"repo\":\"%s\",\"branch\":\"%s\"}",
             s->ssid, s->pass[0] ? "***" : "", s->token[0] ? "***" : "", s->repo,
             s->branch[0] ? s->branch : "main");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t post_config_handler(httpd_req_t* req) {
    char body[800];
    int len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }
    body[len] = '\0';

    cJSON* root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid JSON");
        return ESP_FAIL;
    }

    settings_t s = *settings_get();
    apply_field(root, "ssid", s.ssid, sizeof(s.ssid));
    apply_field(root, "pass", s.pass, sizeof(s.pass));
    apply_field(root, "token", s.token, sizeof(s.token));
    apply_field(root, "repo", s.repo, sizeof(s.repo));
    apply_field(root, "branch", s.branch, sizeof(s.branch));
    cJSON_Delete(root);

    if (s_on_save)
        s_on_save(&s);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
}

esp_err_t web_server_start(web_config_save_cb_t on_save) {
    if (s_server)
        return ESP_OK;

    s_on_save = on_save;

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "httpd_start failed");
        return ESP_FAIL;
    }

    static const httpd_uri_t get_root = {
        .uri = "/", .method = HTTP_GET, .handler = get_root_handler};
    static const httpd_uri_t get_cfg = {
        .uri = "/api/config", .method = HTTP_GET, .handler = get_config_handler};
    static const httpd_uri_t post_cfg = {
        .uri = "/api/config", .method = HTTP_POST, .handler = post_config_handler};
    httpd_register_uri_handler(s_server, &get_root);
    httpd_register_uri_handler(s_server, &get_cfg);
    httpd_register_uri_handler(s_server, &post_cfg);

    ESP_LOGI(TAG, "HTTP server started");
    return ESP_OK;
}

void web_server_stop(void) {
    if (!s_server)
        return;
    httpd_stop(s_server);
    s_server = NULL;
}
