/**
 * @file
 * @brief ESP LCD touch: CST820
 */

#pragma once
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "I2C_Driver.h"
#include "ST7701S.h"
#include "TCA9554PWR.h"
#include "driver/gpio.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

esp_err_t esp_lcd_touch_new_i2c_cst820(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_touch_config_t* config,
                                       esp_lcd_touch_handle_t* tp);

#define ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS (0x15)

#define ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG()                                                 \
    {                                                                                        \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS, .scl_speed_hz = I2C_MASTER_FREQ_HZ, \
        .control_phase_bytes = 1, .dc_bit_offset = 0, .lcd_cmd_bits = 8, .flags = {          \
            .disable_control_phase = 1,                                                      \
        }                                                                                    \
    }

#define I2C_Touch_INT_IO 16
#define I2C_Touch_RST_IO -1

extern esp_lcd_touch_handle_t tp;

void Touch_Init(void);
