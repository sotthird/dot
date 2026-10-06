#pragma once

#include "driver/i2c_master.h"
#include "esp_err.h"

/* Minimal QMI8658 driver: accelerometer only. */

/* Reset the chip and start the accelerometer (+/-4 g, 21 Hz low-power, filtered). */
esp_err_t qmi8658_init(i2c_master_bus_handle_t bus);

/* Read acceleration along the chip's axes, in g. */
esp_err_t qmi8658_read_accel(float* ax, float* ay, float* az);
