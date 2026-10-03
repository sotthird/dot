/* Waveshare ESP32-S3-Touch-LCD-2.1: ST7701S RGB panel, CST820 touch, TCA9554 I/O expander. */

#include "board_priv.h"
#include "cst820.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_lcd_panel_rgb.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "st7701s.h"
#include "tca9554.h"

static const char* TAG = "board";

#define PIN_I2C_SCL 7
#define PIN_I2C_SDA 15
#define PIN_TOUCH_INT 16
#define PIN_BACKLIGHT 6

/* Panel and touch control lines sit on the I/O expander. */
#define EXIO_LCD_RST 1
#define EXIO_TOUCH_RST 2
#define EXIO_LCD_CS 3
#define EXIO_BUZZER 8

/* The panel's 3-wire SPI (init commands only) */
#define PIN_LCD_SPI_MOSI 1
#define PIN_LCD_SPI_SCLK 2

#define LCD_PIXEL_CLOCK_HZ (18 * 1000 * 1000)

#define BACKLIGHT_TIMER LEDC_TIMER_0
#define BACKLIGHT_CHANNEL LEDC_CHANNEL_0
#define BACKLIGHT_DUTY_BITS LEDC_TIMER_13_BIT
#define BACKLIGHT_DUTY_MAX ((1 << BACKLIGHT_DUTY_BITS) - 1)

static void exio_pulse_low(uint8_t pin, int low_ms, int settle_ms) {
    tca9554_set(pin, false);
    vTaskDelay(pdMS_TO_TICKS(low_ms));
    tca9554_set(pin, true);
    vTaskDelay(pdMS_TO_TICKS(settle_ms));
}

static i2c_master_bus_handle_t i2c_init(void) {
    const i2c_master_bus_config_t cfg = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM_0,
        .scl_io_num = PIN_I2C_SCL,
        .sda_io_num = PIN_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus;
    ESP_ERROR_CHECK(i2c_new_master_bus(&cfg, &bus));
    return bus;
}

static esp_lcd_panel_handle_t panel_init(void) {
    /* Pulse reset, then hold the panel's SPI chip-select low while its registers are written */
    exio_pulse_low(EXIO_LCD_RST, 10, 50);

    tca9554_set(EXIO_LCD_CS, true);
    vTaskDelay(pdMS_TO_TICKS(10));
    tca9554_set(EXIO_LCD_CS, false);
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_ERROR_CHECK(st7701s_init_registers(PIN_LCD_SPI_MOSI, PIN_LCD_SPI_SCLK, -1));

    const esp_lcd_rgb_panel_config_t cfg = {
        .data_width = 16,
        .num_fbs = 1,
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .disp_gpio_num = -1,
        .pclk_gpio_num = 41,
        .vsync_gpio_num = 39,
        .hsync_gpio_num = 38,
        .de_gpio_num = 40,
        .data_gpio_nums = {5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17},
        .timings =
            {
                .pclk_hz = LCD_PIXEL_CLOCK_HZ,
                .h_res = BOARD_LCD_H_RES,
                .v_res = BOARD_LCD_V_RES,
                .hsync_back_porch = 10,
                .hsync_front_porch = 50,
                .hsync_pulse_width = 8,
                .vsync_back_porch = 8,
                .vsync_front_porch = 8,
                .vsync_pulse_width = 3,
                .flags.pclk_active_neg = false,
            },
        .flags.fb_in_psram = true,
    };
    esp_lcd_panel_handle_t panel;
    ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&cfg, &panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel));

    tca9554_set(EXIO_LCD_CS, false);
    vTaskDelay(pdMS_TO_TICKS(10));
    tca9554_set(EXIO_LCD_CS, true);
    vTaskDelay(pdMS_TO_TICKS(50));
    return panel;
}

static void backlight_init(void) {
    const ledc_timer_config_t timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = BACKLIGHT_DUTY_BITS,
        .timer_num = BACKLIGHT_TIMER,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer));

    const ledc_channel_config_t channel = {
        .gpio_num = PIN_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = BACKLIGHT_CHANNEL,
        .timer_sel = BACKLIGHT_TIMER,
        .duty = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel));
}

void board_set_brightness(uint8_t percent) {
    if (percent > 100)
        percent = 100;
    /* Duty rises 81 counts per percent up to full scale; 0 is fully off */
    uint32_t duty = percent ? BACKLIGHT_DUTY_MAX - 81 * (100 - percent) : 0;
    ledc_set_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, BACKLIGHT_CHANNEL);
}

static esp_lcd_touch_handle_t touch_init(i2c_master_bus_handle_t bus) {
    exio_pulse_low(EXIO_TOUCH_RST, 10, 50);

    esp_lcd_panel_io_handle_t io;
    const esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io));

    const esp_lcd_touch_config_t cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = PIN_TOUCH_INT,
    };
    esp_lcd_touch_handle_t touch;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst820(io, &cfg, &touch));
    return touch;
}

void board_hw_init(board_hw_t* hw) {
    i2c_master_bus_handle_t bus = i2c_init();
    ESP_ERROR_CHECK(tca9554_init(bus));
    tca9554_set(EXIO_BUZZER, false);

    hw->panel = panel_init();
    backlight_init();
    board_set_brightness(BOARD_DEFAULT_BRIGHTNESS);

    hw->touch = touch_init(bus);
    ESP_LOGI(TAG, "ESP32-S3-Touch-LCD-2.1 ready");
}
