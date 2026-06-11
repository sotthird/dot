#pragma once

#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

#define I2C_Touch_SCL_IO 7
#define I2C_Touch_SDA_IO 15
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 400000
#define I2C_MASTER_TIMEOUT_MS 1000

extern i2c_master_bus_handle_t i2c_bus_handle;

void I2C_Init(void);

esp_err_t I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t* Reg_data,
                    uint32_t Length);
esp_err_t I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t* Reg_data, uint32_t Length);
