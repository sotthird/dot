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

void Driver_Loop(void *parameter)
{
    while(1)
    {
        // QMI8658_Loop();
        // RTC_Loop();
        // BAT_Get_Volts();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    vTaskDelete(NULL);
}

void Driver_Init(void)
{
    // Flash_Searching();
    // BAT_Init();
    I2C_Init();
    // PCF85063_Init();
    // QMI8658_Init();
    EXIO_Init();                    // Example Initialize EXIO
    xTaskCreatePinnedToCore(
        Driver_Loop, 
        "Other Driver task",
        4096, 
        NULL, 
        3, 
        NULL, 
        0);
}

static void ci_task(void *arg) {
    (void)arg;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5));
        ci_app_update();
        lv_timer_handler();
    }
}


void app_main(void) {
    // I2C_Init();
    // EXIO_Init();
    // Wireless_Init();
    Driver_Init();
    LCD_Init();
    Touch_Init();
    LVGL_Init();

    ci_app_init();
    ci_app_create_ui();

    xTaskCreatePinnedToCore(ci_task, "ci", 8192, NULL, 2, NULL, 1);

}
