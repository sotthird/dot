/* Waveshare ESP32-S3-Touch-AMOLED-2.16: CO5300 QSPI AMOLED, CST9217 touch, no backlight pin
 * (brightness is a panel command). */

#include "board_priv.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_lcd_co5300.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "board";

#define PIN_LCD_CS 12
#define PIN_LCD_SCLK 38
#define PIN_LCD_D0 4
#define PIN_LCD_D1 5
#define PIN_LCD_D2 6
#define PIN_LCD_D3 7
#define PIN_LCD_RST 39

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

void board_hw_init(board_hw_t* hw) {
    hw->panel_io = panel_init();
    hw->panel = s_panel;
    ESP_LOGI(TAG, "CO5300 panel ready");

    board_set_brightness(BOARD_DEFAULT_BRIGHTNESS);

    /* Touch (hw->touch) arrives in a later phase; LVGL runs without an input device until then. */
}
