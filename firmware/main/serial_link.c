#include "serial_link.h"

#include <stdio.h>
#include <string.h>

#include "app.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define LINE_BYTES 256

static const char* TAG = "serial";

typedef struct {
    uint8_t* dst;  // NULL: payload is too large for the image buffer and is discarded
    size_t received;
    size_t remaining;
    int w, h;
    uint32_t color;
} image_rx_t;

static void image_rx_finish(image_rx_t* rx) {
    if (rx->dst)
        app_handle_image(rx->dst, rx->w, rx->h, rx->color);
}

static void on_line(const char* line, image_rx_t* rx) {
    unsigned nbytes, color;
    if (sscanf(line, "IMG:%dx%d:%u:%x", &rx->w, &rx->h, &nbytes, &color) == 4) {
        rx->color = color;
        rx->dst = app_image_buffer(nbytes);
        rx->received = 0;
        rx->remaining = nbytes;
        if (nbytes == 0)
            image_rx_finish(rx);
    } else {
        app_handle_line(line);
    }
}

static void reader_task(void* arg) {
    char line[LINE_BYTES];
    size_t len = 0;
    image_rx_t rx = {0};
    uint8_t chunk[64];

    for (;;) {
        int n = usb_serial_jtag_read_bytes(chunk, sizeof(chunk), pdMS_TO_TICKS(100));

        for (int i = 0; i < n;) {
            if (rx.remaining > 0) {
                size_t take = (size_t)(n - i) < rx.remaining ? (size_t)(n - i) : rx.remaining;
                if (rx.dst)
                    memcpy(rx.dst + rx.received, &chunk[i], take);
                rx.received += take;
                rx.remaining -= take;
                i += (int)take;
                if (rx.remaining == 0)
                    image_rx_finish(&rx);
                continue;
            }

            char c = (char)chunk[i++];
            if (c != '\n' && c != '\r') {
                if (len < sizeof(line) - 1)
                    line[len++] = c;
            } else if (len > 0) {
                line[len] = '\0';
                len = 0;
                on_line(line, &rx);
            }
        }
    }
}

void serial_link_start(void) {
    usb_serial_jtag_driver_config_t cfg = {.rx_buffer_size = 1024, .tx_buffer_size = 1024};
    esp_err_t err = usb_serial_jtag_driver_install(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE)
        ESP_LOGE(TAG, "USB serial driver install failed: %s", esp_err_to_name(err));

    xTaskCreatePinnedToCore(reader_task, "serial_link", 4096, NULL, 3, NULL, 0);
}

void serial_link_send(const char* msg) {
    usb_serial_jtag_write_bytes((const uint8_t*)msg, strlen(msg), pdMS_TO_TICKS(100));
}
