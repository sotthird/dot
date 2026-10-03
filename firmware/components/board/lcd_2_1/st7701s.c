/*
 * SPDX-FileCopyrightText: Waveshare
 * SPDX-License-Identifier: Apache-2.0
 *
 * ST7701S register setup for the 2.1" 480x480 panel, from Waveshare's demo firmware.
 * The pixel data itself goes over the RGB interface; this SPI link only carries the
 * initialisation commands.
 */

#include "st7701s.h"

#include "driver/spi_master.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define SPI_HOST_ID SPI2_HOST
#define SPI_CLOCK_HZ 4000000

#define delay_ms(ms) vTaskDelay(pdMS_TO_TICKS(ms))

static spi_device_handle_t dev;

/* 9-bit frames: a D/C bit (0 = command, 1 = data) followed by one byte. */
static void write_frame(uint32_t dc, uint8_t byte) {
    spi_transaction_t t = {.cmd = dc, .addr = byte};
    spi_device_transmit(dev, &t);
}

static void cmd(uint8_t byte) {
    write_frame(0, byte);
}

static void data(uint8_t byte) {
    write_frame(1, byte);
}

static void send_init_sequence(void) {
    cmd(0xFF);
    data(0x77);
    data(0x01);
    data(0x00);
    data(0x00);
    data(0x10);

    cmd(0xC0);
    data(0x3B);
    data(0x00);

    cmd(0xC1);
    data(0x0B);
    data(0x02);

    cmd(0xC2);
    data(0x07);
    data(0x02);

    cmd(0xCC);
    data(0x10);

    cmd(0xCD);
    data(0x08);

    cmd(0xB0);
    data(0x00);
    data(0x11);
    data(0x16);
    data(0x0e);
    data(0x11);
    data(0x06);
    data(0x05);
    data(0x09);
    data(0x08);
    data(0x21);
    data(0x06);
    data(0x13);
    data(0x10);
    data(0x29);
    data(0x31);
    data(0x18);

    cmd(0xB1);
    data(0x00);
    data(0x11);
    data(0x16);
    data(0x0e);
    data(0x11);
    data(0x07);
    data(0x05);
    data(0x09);
    data(0x09);
    data(0x21);
    data(0x05);
    data(0x13);
    data(0x11);
    data(0x2a);
    data(0x31);
    data(0x18);

    cmd(0xFF);
    data(0x77);
    data(0x01);
    data(0x00);
    data(0x00);
    data(0x11);

    cmd(0xB0);
    data(0x6d);

    cmd(0xB1);
    data(0x37);

    cmd(0xB2);
    data(0x81);

    cmd(0xB3);
    data(0x80);

    cmd(0xB5);
    data(0x43);

    cmd(0xB7);
    data(0x85);

    cmd(0xB8);
    data(0x20);

    cmd(0xC1);
    data(0x78);

    cmd(0xC2);
    data(0x78);

    cmd(0xD0);
    data(0x88);

    cmd(0xE0);
    data(0x00);
    data(0x00);
    data(0x02);

    cmd(0xE1);
    data(0x03);
    data(0xA0);
    data(0x00);
    data(0x00);
    data(0x04);
    data(0xA0);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x20);
    data(0x20);

    cmd(0xE2);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);

    cmd(0xE3);
    data(0x00);
    data(0x00);
    data(0x11);
    data(0x00);

    cmd(0xE4);
    data(0x22);
    data(0x00);

    cmd(0xE5);
    data(0x05);
    data(0xEC);
    data(0xA0);
    data(0xA0);
    data(0x07);
    data(0xEE);
    data(0xA0);
    data(0xA0);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);

    cmd(0xE6);
    data(0x00);
    data(0x00);
    data(0x11);
    data(0x00);

    cmd(0xE7);
    data(0x22);
    data(0x00);

    cmd(0xE8);
    data(0x06);
    data(0xED);
    data(0xA0);
    data(0xA0);
    data(0x08);
    data(0xEF);
    data(0xA0);
    data(0xA0);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);
    data(0x00);

    cmd(0xEB);
    data(0x00);
    data(0x00);
    data(0x40);
    data(0x40);
    data(0x00);
    data(0x00);
    data(0x00);

    cmd(0xED);
    data(0xFF);
    data(0xFF);
    data(0xFF);
    data(0xBA);
    data(0x0A);
    data(0xBF);
    data(0x45);
    data(0xFF);
    data(0xFF);
    data(0x54);
    data(0xFB);
    data(0xA0);
    data(0xAB);
    data(0xFF);
    data(0xFF);
    data(0xFF);

    cmd(0xEF);
    data(0x10);
    data(0x0D);
    data(0x04);
    data(0x08);
    data(0x3F);
    data(0x1F);

    cmd(0xFF);
    data(0x77);
    data(0x01);
    data(0x00);
    data(0x00);
    data(0x13);

    cmd(0xEF);
    data(0x08);

    cmd(0xFF);
    data(0x77);
    data(0x01);
    data(0x00);
    data(0x00);
    data(0x00);

    cmd(0x36);
    data(0x00);

    cmd(0x3A);
    data(0x66);

    cmd(0x11);
    delay_ms(480);

    cmd(0x20);
    delay_ms(120);
    cmd(0x29);
}

esp_err_t st7701s_init_registers(gpio_num_t mosi, gpio_num_t sclk, gpio_num_t cs) {
    const spi_bus_config_t bus = {
        .mosi_io_num = mosi,
        .miso_io_num = -1,
        .sclk_io_num = sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(SPI_HOST_ID, &bus, SPI_DMA_CH_AUTO), "st7701s",
                        "SPI bus init failed");

    const spi_device_interface_config_t dev_cfg = {
        .command_bits = 1,
        .address_bits = 8,
        .clock_speed_hz = SPI_CLOCK_HZ,
        .mode = 0,
        .spics_io_num = cs,
        .queue_size = 1,
    };
    ESP_RETURN_ON_ERROR(spi_bus_add_device(SPI_HOST_ID, &dev_cfg, &dev), "st7701s",
                        "SPI device add failed");

    send_init_sequence();

    spi_bus_remove_device(dev);
    spi_bus_free(SPI_HOST_ID);
    return ESP_OK;
}
