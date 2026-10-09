#pragma once

#include <stdbool.h>

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Minimal AXP2101 power management driver: battery level and charge state only. */

esp_err_t axp2101_init(i2c_master_bus_handle_t bus);

/* percent is -1 when no battery is connected. */
esp_err_t axp2101_read(int* percent, bool* charging, bool* usb_power);
