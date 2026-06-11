# LCD_Driver (ST7701S)

Brings up the **ST7701S** RGB-interface LCD controller driving the board's
480×480 circular IPS panel, and manages backlight brightness via PWM.

## What it does

- Creates an SPI connection to the ST7701S (used only to send its initialization
  command sequence — pixel data goes over the parallel **RGB565** interface).
- Sends the full vendor init sequence for the 2.1" panel (`ST7701S_screen_init`,
  `type == 1`) — gamma, voltage, and timing registers specific to this display.
- Configures the ESP32-S3's RGB LCD peripheral (`esp_lcd_rgb_panel`) with the
  panel's resolution, timings, and the 16 parallel data pins.
- Sets up a PWM channel (LEDC) to control backlight brightness.
- Drives LCD reset and chip-select through the IO expander (see
  [`EXIO`](../EXIO/README.md)) since those lines aren't wired to direct GPIOs.

## Public API

| Function | Description |
|---|---|
| `ST7701S_handle ST7701S_newObject(...)` | Allocates an ST7701S handle and sets up the SPI bus/device used to send init commands. |
| `void ST7701S_screen_init(ST7701S_handle, unsigned char type)` | Sends the register init sequence for the given panel type (`1` = 2.1" panel). |
| `void ST7701S_delObject(ST7701S_handle)` | Frees an ST7701S handle. |
| `void ST7701S_WriteCommand/WriteData(...)` | Low-level SPI command/data byte send. |
| `esp_err_t ST7701S_reset(void)` | Pulses the LCD reset line (via `EXIO1`). |
| `esp_err_t ST7701S_CS_EN/CS_Dis(void)` | Drives the LCD SPI chip-select (via `EXIO3`). |
| `void LCD_Init(void)` | Full bring-up: resets the panel, sends the init sequence, configures the RGB peripheral, and enables the backlight. Call this once at startup. |
| `void Backlight_Init(void)` | Configures the LEDC PWM timer/channel for the backlight pin. |
| `void Set_Backlight(uint8_t Light)` | Sets backlight brightness on a `0–100` scale. |

## Hardware connections

| Signal | Pin / source |
|---|---|
| SPI MOSI / SCLK | GPIO 1 / GPIO 2 (SPI2_HOST, 4 MHz — used only for init commands) |
| RGB data (B0–B4, G0–G5, R0–R4) | GPIO 5,45,48,47,21 / 14,13,12,11,10,9 / 46,3,8,18,17 |
| HSYNC / VSYNC / DE / PCLK | GPIO 38 / 39 / 40 / 41 |
| Backlight PWM | GPIO 6 (LEDC timer 0, channel 0) |
| Reset | via IO expander `EXIO1` |
| Chip-select | via IO expander `EXIO3` |

## Settings / configurable values

| Define | Value | Meaning |
|---|---|---|
| `EXAMPLE_LCD_H_RES` / `EXAMPLE_LCD_V_RES` | `480` / `480` | Panel resolution |
| `EXAMPLE_LCD_PIXEL_CLOCK_HZ` | `18 MHz` | RGB pixel clock |
| `LEDC_ResolutionRatio` | 13-bit | PWM duty resolution (`LEDC_MAX_Duty` = 8191) |
| `LEDC_TEST_DUTY` | `4000` | Initial test duty cycle |
| `Backlight_MAX` | `100` | Upper bound of the public 0–100 brightness scale |
| `LCD_Backlight` | `70` | Default backlight level applied at init |
| RGB timing (`hsync_bp/fp/pw`, `vsync_bp/fp/pw`) | `10/50/8`, `8/8/3` | Panel-specific sync timing, taken from the vendor datasheet |

Brightness-to-duty mapping used by `Set_Backlight`:

```
duty = 8191 - (81 * (100 - Light))
```

## Notes

- The long register table in `ST7701S_screen_init` (commands `0xFF`, `0xC0`,
  `0xC1`, `0xB0`, `0xB1`, gamma tables `0xE0`–`0xE8`, etc.) is the panel
  manufacturer's required init sequence for this specific 2.1" variant — don't
  reorder or trim it without datasheet guidance.
- Frame buffers are allocated in **PSRAM** (1 or 2 depending on
  `CONFIG_EXAMPLE_DOUBLE_FB`) since 480×480×2 bytes won't fit in internal SRAM.
- An optional VSYNC callback synchronizes LVGL rendering to the display refresh
  (gated by `CONFIG_EXAMPLE_AVOID_TEAR_EFFECT_WITH_SEM`) to reduce tearing.
- Rendering uses `LV_DISPLAY_RENDER_MODE_PARTIAL` — see [`LVGL_Driver`](../LVGL_Driver/README.md).
