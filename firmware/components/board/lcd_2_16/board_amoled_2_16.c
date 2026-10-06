/* Waveshare ESP32-S3-Touch-AMOLED-2.16: CO5300 QSPI AMOLED, CST9217 touch, no backlight pin
 * (brightness is a panel command). */

#include "board_priv.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_lcd_touch_cst9217.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qmi8658.h"

static const char* TAG = "board";

#define PIN_LCD_CS 12
#define PIN_LCD_SCLK 38
#define PIN_LCD_D0 4
#define PIN_LCD_D1 5
#define PIN_LCD_D2 6
#define PIN_LCD_D3 7
#define PIN_LCD_RST 39

#define PIN_I2C_SCL 14
#define PIN_I2C_SDA 15
#define PIN_TOUCH_INT 11
#define PIN_TOUCH_RST 40

#define I2C_CLOCK_HZ 400000

#define LCD_SPI_HOST SPI2_HOST

static esp_lcd_panel_handle_t s_panel;

/* Waveshare's init sequence. 0x36 (MADCTL) is the panel's scan direction: Waveshare ships 0xA0
 * (swap X/Y + mirror Y), which showed the image rotated 90 degrees here, so it is 0x00. */
static const co5300_lcd_init_cmd_t init_cmds[] = {
    {0x11, (uint8_t[]){0x00}, 0, 600}, /* sleep out */

    {0xFE, (uint8_t[]){0x20}, 1, 0},
    {0x19, (uint8_t[]){0x10}, 1, 0},
    {0x1C, (uint8_t[]){0xA0}, 1, 0},

    {0xFE, (uint8_t[]){0x00}, 1, 0},
    {0xC4, (uint8_t[]){0x80}, 1, 0},
    {0x3A, (uint8_t[]){0x55}, 1, 0}, /* RGB565 */
    {0x35, (uint8_t[]){0x00}, 1, 0},
    {0x53, (uint8_t[]){0x20}, 1, 0},
    {0x51, (uint8_t[]){0xFF}, 1, 0}, /* brightness */
    {0x63, (uint8_t[]){0xFF}, 1, 0},
    {0x2A, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x2B, (uint8_t[]){0x00, 0x00, 0x01, 0xDF}, 4, 0},
    {0x36, (uint8_t[]){0x00}, 1, 0},
    {0x29, (uint8_t[]){0x00}, 0, 600}, /* display on */
};

static esp_lcd_panel_io_handle_t panel_init(void) {
    const spi_bus_config_t bus_cfg = CO5300_PANEL_BUS_QSPI_CONFIG(
        PIN_LCD_SCLK, PIN_LCD_D0, PIN_LCD_D1, PIN_LCD_D2, PIN_LCD_D3, BOARD_DRAW_BUF_BYTES);
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_SPI_HOST, &bus_cfg, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = CO5300_PANEL_IO_QSPI_CONFIG(PIN_LCD_CS, NULL, NULL);
    io_cfg.trans_queue_depth = 3;
    esp_lcd_panel_io_handle_t io;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_SPI_HOST, &io_cfg, &io));

    const co5300_vendor_config_t vendor_cfg = {
        .init_cmds = init_cmds,
        .init_cmds_size = sizeof(init_cmds) / sizeof(init_cmds[0]),
        .flags.use_qspi_interface = 1,
    };
    const esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = PIN_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .vendor_config = (void*)&vendor_cfg,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_co5300(io, &panel_cfg, &s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));
    return io;
}

void board_set_brightness(uint8_t percent) {
    if (!s_panel)
        return;
    if (percent > 100)
        percent = 100;
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_co5300_set_brightness(s_panel, percent));
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

static esp_lcd_touch_handle_t touch_init(i2c_master_bus_handle_t bus) {
    esp_lcd_panel_io_handle_t io;
    esp_lcd_panel_io_i2c_config_t io_cfg = ESP_LCD_TOUCH_IO_I2C_CST9217_CONFIG();
    io_cfg.scl_speed_hz = I2C_CLOCK_HZ;
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(bus, &io_cfg, &io));

    /* esp_lcd_touch mirrors first, then swaps. The controller's axes are transposed relative to
     * the panel, so swap and mirror X (what Clawdmeter uses for this board). The driver pulses
     * the reset line itself. */
    const esp_lcd_touch_config_t cfg = {
        .x_max = BOARD_LCD_H_RES,
        .y_max = BOARD_LCD_V_RES,
        .rst_gpio_num = PIN_TOUCH_RST,
        .int_gpio_num = PIN_TOUCH_INT,
        .levels = {.reset = 0, .interrupt = 0},
        .flags = {.swap_xy = 1, .mirror_x = 1, .mirror_y = 0},
    };
    esp_lcd_touch_handle_t touch;
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst9217(io, &cfg, &touch));
    return touch;
}

static esp_err_t read_accel_xy(float* ax, float* ay) {
    float az;
    return qmi8658_read_accel(ax, ay, &az);
}

void board_hw_init(board_hw_t* hw) {
    hw->panel_io = panel_init();
    hw->panel = s_panel;
    ESP_LOGI(TAG, "CO5300 panel ready");

    board_set_brightness(BOARD_DEFAULT_BRIGHTNESS);

    i2c_master_bus_handle_t bus = i2c_init();
    hw->touch = touch_init(bus);

    /* Without the accelerometer the display just stays in its default orientation */
    if (qmi8658_init(bus) == ESP_OK)
        hw->read_accel = read_accel_xy;
    else
        ESP_LOGW(TAG, "accelerometer not found, auto-rotation disabled");
    ESP_LOGI(TAG, "ESP32-S3-Touch-AMOLED-2.16 ready");
}
