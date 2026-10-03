#include "app.h"
#include "board.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "sdkconfig.h"
#include "serial_link.h"

#if CONFIG_DOT_APP_CI
#include "apps/ci/ci.h"
#define ACTIVE_APP ci_app
#elif CONFIG_DOT_APP_SPOTIFY
#include "apps/spotify/spotify.h"
#define ACTIVE_APP spotify_app
#elif CONFIG_DOT_APP_CPU
#include "apps/cpu/cpu.h"
#define ACTIVE_APP cpu_app
#endif

#define UI_TASK_MAX_SLEEP_MS 5

static void ui_task(void* arg) {
    for (;;) {
        app_update_all();
        uint32_t sleep_ms = lv_timer_handler();

        /* lv_timer_handler() can return 0 while an animation runs; always yield at least one
         * tick so this task cannot starve the idle task and trip the watchdog. */
        if (sleep_ms > UI_TASK_MAX_SLEEP_MS)
            sleep_ms = UI_TASK_MAX_SLEEP_MS;
        TickType_t delay = pdMS_TO_TICKS(sleep_ms);
        vTaskDelay(delay ? delay : 1);
    }
}

void app_main(void) {
    board_init();

    app_init();
    app_register(&ACTIVE_APP);
    app_create_uis();

    serial_link_start();
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, 2, NULL, 1);
}
