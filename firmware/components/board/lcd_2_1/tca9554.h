#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* TCA9554 8-bit I/O expander. Pins are numbered 1-8 (EXIO1-EXIO8). */

/* Configure every pin as an output and remember their current levels. */
esp_err_t tca9554_init(i2c_master_bus_handle_t bus);

esp_err_t tca9554_set(uint8_t pin, bool level);
