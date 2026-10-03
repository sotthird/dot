# board

Everything that depends on the hardware, behind two calls from `board.h`:

```c
void board_init(void);                      // display + touch + LVGL, ready for screens
void board_set_brightness(uint8_t percent); // 0-100
```

`lcd_2_1/` implements `board_hw_init()` (reset and configure the panel and touch controller)
and `board_set_brightness()`; `lvgl_port.c` connects the result to LVGL. Supporting another
board means adding a directory that implements those two functions and listing its sources
in `CMakeLists.txt`.

## lcd_2_1: Waveshare ESP32-S3-Touch-LCD-2.1

480x480 round IPS, ST7701S over 16-bit parallel RGB (framebuffer in PSRAM), CST820 touch,
PWM backlight.

| Signal | Pin |
|---|---|
| I2C SCL / SDA | GPIO 7 / 15 |
| Touch INT | GPIO 16 |
| Backlight (PWM) | GPIO 6 |
| Panel init SPI (MOSI / SCLK) | GPIO 1 / 2, used only while sending the init sequence |
| RGB HSYNC / VSYNC / DE / PCLK | GPIO 38 / 39 / 40 / 41 |
| RGB data D0-D15 | GPIO 5, 45, 48, 47, 21, 14, 13, 12, 11, 10, 9, 46, 3, 8, 18, 17 |
| LCD reset / LCD CS / touch reset / buzzer | TCA9554 I/O expander (0x20), EXIO1 / 3 / 2 / 8 |
