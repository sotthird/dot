#include "app.h"
#include "apps/ci/ci.h"
#include "apps/cpu/cpu.h"
#include "apps/equity/equity.h"
#include "apps/spotify/spotify.h"
#include "board.h"
#include "brightness.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "radio.h"
#include "sdkconfig.h"
#include "serial_link.h"
#include "status_ui.h"

#define UI_TASK_MAX_SLEEP_MS 5

static void ui_task(void* arg) {
    for (;;) {
        app_update_active();
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

    /* Swipe order. The first enabled app is shown at startup. */
#if CONFIG_DOT_APP_CI
    app_register(&ci_app);
#endif
#if CONFIG_DOT_APP_SPOTIFY
    app_register(&spotify_app);
#endif
#if CONFIG_DOT_APP_CPU
    app_register(&cpu_app);
#endif
#if CONFIG_DOT_APP_EQUITY
    app_register(&equity_app);
#endif
    app_create_uis();
#if CONFIG_DOT_BOARD_AMOLED_2_16
    status_ui_start();
    brightness_start();
    radio_start();
#endif

    serial_link_start();
    xTaskCreatePinnedToCore(ui_task, "ui", 8192, NULL, 2, NULL, 1);
}
