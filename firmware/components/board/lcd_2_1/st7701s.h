#pragma once

#include "driver/gpio.h"
#include "esp_err.h"

/* Send the register init sequence for the 2.1" panel over a temporary 3-wire SPI link,
 * then release the SPI bus. The chip-select is pulled low externally when cs is -1. */
esp_err_t st7701s_init_registers(gpio_num_t mosi, gpio_num_t sclk, gpio_num_t cs);
