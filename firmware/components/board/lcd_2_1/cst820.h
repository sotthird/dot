#pragma once

#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch.h"

esp_err_t esp_lcd_touch_new_i2c_cst820(const esp_lcd_panel_io_handle_t io,
                                       const esp_lcd_touch_config_t* config,
                                       esp_lcd_touch_handle_t* tp);

#define ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS (0x15)

#define ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG()                                        \
    {                                                                               \
        .dev_addr = ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS, .scl_speed_hz = 400000,    \
        .control_phase_bytes = 1, .dc_bit_offset = 0, .lcd_cmd_bits = 8, .flags = { \
            .disable_control_phase = 1,                                             \
        }                                                                           \
    }
