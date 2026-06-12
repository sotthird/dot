#include <stdio.h>
#include <string.h>

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
#include "apps/ci_app.h"
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
                    idx = 0;

                    int w, h;
                    unsigned int nbytes;
                    unsigned int color;
                    if (sscanf(line, "IMG:%dx%d:%u:%x", &w, &h, &nbytes, &color) == 4) {
                        uint8_t* dst = app_get_image_buffer(nbytes);
                        size_t got = 0;

                        int remaining_in_chunk = len - (i + 1);
                        if (remaining_in_chunk > 0) {
                            size_t take = (size_t)remaining_in_chunk < nbytes
                                              ? (size_t)remaining_in_chunk
                                              : nbytes;
                            if (dst)
                                memcpy(dst, &raw[i + 1], take);
                            got = take;
                            i += (int)take;
                        }

                        while (got < nbytes) {
                            uint8_t discard[64];
                            size_t want = nbytes - got;
                            uint8_t* read_dst;
                            if (dst) {
                                read_dst = dst + got;
                            } else {
                                read_dst = discard;
                                if (want > sizeof(discard))
                                    want = sizeof(discard);
                            }
                            int r = usb_serial_jtag_read_bytes(read_dst, want, pdMS_TO_TICKS(1000));
                            if (r > 0)
                                got += (size_t)r;
                        }

                        if (dst)
                            app_handle_image(dst, w, h, color);
                    } else {
                        app_handle_line(line);
                    }
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
        /* Always block at least one tick. When an animation is active
         * lv_timer_handler() can return 0, and a 0-length delay would let this
         * task busy-loop and starve the core-1 IDLE task, tripping the WDT. */
        TickType_t delay = pdMS_TO_TICKS(sleep_ms);
        if (delay == 0)
            delay = 1;
        vTaskDelay(delay);
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

    // app_register(&spotify_app);
    app_register(&ci_app);

    /* Only one screen is active at a time (each create_ui does its own
     * lv_screen_load), so register exactly one app. To use the CI status orb,
     * swap the line above for: app_register(&ci_app); */

    xSemaphoreTake(lvgl_mux, portMAX_DELAY);
    app_start_all();
    xSemaphoreGive(lvgl_mux);

    xTaskCreatePinnedToCore(lvgl_task, "lvgl", 8192, NULL, 2, NULL, 1);
    xTaskCreatePinnedToCore(serial_reader_task, "serial_reader", 4096, NULL, 3, NULL, 0);
}
