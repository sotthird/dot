#include <stdio.h>

#include "CST820.h"
#include "LVGL_Driver.h"
#include "ST7701S.h"
#include "TCA9554PWR.h"
// #include "Wireless.h"
#include "apps/ci_app.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#define TAG "main"

static void ci_task(void* arg) {
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10));
        ci_app_update();
        lv_timer_handler();
    }
}

void app_main(void) {
    I2C_Init();
    EXIO_Init();
    LCD_Init();
    Touch_Init();
    LVGL_Init();

    ci_app_init();
    ci_app_create_ui();

    xTaskCreatePinnedToCore(ci_task, "ci", 8192, NULL, 2, NULL, 1);
}
