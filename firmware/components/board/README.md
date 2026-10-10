# board

Everything that depends on the hardware, behind two calls from `board.h`:

```c
void board_init(void);                      // display + touch + LVGL, ready for screens
void board_set_brightness(uint8_t percent); // 0-100
```

Each board directory implements `board_hw_init()` (reset and configure the panel and touch
controller) and `board_set_brightness()`; `lvgl_port.c` connects the result to LVGL. Supporting
another board means adding a directory that implements those two functions, listing its sources
in `CMakeLists.txt` and adding a choice to `Kconfig.projbuild`.

Pick the board with `idf.py menuconfig` → **Dot: Board**. A board that has an accelerometer can
also set `board_hw_t.read_accel`, which enables auto-rotation (see below).

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

## amoled_2_16: Waveshare ESP32-S3-Touch-AMOLED-2.16

480x480 AMOLED, CO5300 over QSPI, CST9217 touch, QMI8658 accelerometer. There is no backlight
pin and no I/O expander: brightness is a panel command. Code lives in `lcd_2_16/`.

| Signal | Pin |
|---|---|
| LCD QSPI CS / CLK | GPIO 12 / 38 |
| LCD QSPI D0-D3 | GPIO 4, 5, 6, 7 |
| LCD reset | GPIO 39 |
| I2C SCL / SDA | GPIO 14 / 15 |
| Touch INT / reset | GPIO 11 / 40 |
| Touch address | 0x5A |
| Accelerometer address | 0x6B |
| Power management (AXP2101) address | 0x34 |

The panel and touch drivers are Espressif's `esp_lcd_co5300` and Waveshare's
`esp_lcd_touch_cst9217`, fetched by the component manager. The init sequence is Waveshare's, with
the scan-direction register (`0x36`) set to `0x00` so the image is upright.

- **Auto-rotation** (menuconfig: *Auto-rotate the display*, on by default). The accelerometer picks
  one of four orientations, ignoring tilts under about 30 degrees and a flat board, and a new one
  must hold for 300 ms. The picture is then turned in software as each strip is flushed, and touch
  is mapped back through the same rotation. The screen blanks, redraws and fades back in.
- **Idle dimming** (menuconfig: *Dim the display after this many idle seconds*, 300 s to 15% by
  default, 0 turns it off). A lit AMOLED pixel wears out and apps like `ci` hold one color for
  hours. Only touch and picking the board up count as activity; serial data from the host does not.
- **Brightness** defaults to 30% (`BOARD_DEFAULT_BRIGHTNESS` in `board_priv.h`), far dimmer than the
  LCD board needs for the same look.
