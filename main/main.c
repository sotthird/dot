#include "CST820.h"
#include "LVGL_Driver.h"
#include "ST7701S.h"
#include "TCA9554PWR.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"

/* App registry */
#include "app.h"
#include "apps/cpu_app.h"
#include "apps/spotify_app.h"

#define TAG "main"
#define LVGL_MAX_SLEEP_MS 5

static SemaphoreHandle_t lvgl_mux = NULL;

static void serial_reader_task(void* arg) {
    (void)arg;
    char line[256];
    int idx = 0;
    uint8_t raw[64];

    while (1) {
        int len = usb_serial_jtag_read_bytes(raw, sizeof(raw), pdMS_TO_TICKS(100));
        if (len == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        for (int i = 0; i < len; i++) {
            char c = (char)raw[i];
            if (c == '\n' || c == '\r') {
                if (idx > 0) {
                    line[idx] = '\0';
                    app_handle_line(line);
                    idx = 0;
                }
            } else if (idx < (int)sizeof(line) - 1) {
                line[idx++] = c;
            }
        }
    }
}

static void lvgl_task(void* arg) {
    (void)arg;
    while (1) {
        uint32_t sleep_ms = LVGL_MAX_SLEEP_MS;
        if (xSemaphoreTake(lvgl_mux, pdMS_TO_TICKS(LVGL_MAX_SLEEP_MS)) == pdTRUE) {
            app_update_all();
            sleep_ms = lv_timer_handler();
            xSemaphoreGive(lvgl_mux);
        }
        if (sleep_ms > LVGL_MAX_SLEEP_MS)
            sleep_ms = LVGL_MAX_SLEEP_MS;
        if (sleep_ms > 0)
            vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    }
}

static void sensor_init(void) {
    I2C_Init();
    EXIO_Init();
}

static void display_init(void) {
    LCD_Init();
    Touch_Init();
    LVGL_Init();
}

void app_main(void) {
    usb_serial_jtag_driver_config_t usb_cfg = {
        .rx_buffer_size = 1024,
        .tx_buffer_size = 1024,
    };
    esp_err_t ret = usb_serial_jtag_driver_install(&usb_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "USB JTAG driver install failed: %s", esp_err_to_name(ret));

    sensor_init();
    display_init();

    lvgl_mux = xSemaphoreCreateMutex();
    assert(lvgl_mux);

    app_registry_init();

    app_register(&spotify_app);

    xSemaphoreTake(lvgl_mux, portMAX_DELAY);
    app_start_all();
    xSemaphoreGive(lvgl_mux);

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(serial_reader_task, "serial_reader", 4096, NULL, 3, NULL, 0);
}
