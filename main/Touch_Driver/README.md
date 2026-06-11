# Touch_Driver (CST820)

Driver for the **CST820** capacitive touch controller (Hynitron), plus the
generic `esp_lcd_touch` abstraction layer it implements against. This is what
feeds touch coordinates into LVGL (see [`LVGL_Driver`](../LVGL_Driver/README.md)).

## What it does

- Talks to the CST820 over the shared I2C bus at address `0x15`.
- Drives the controller's reset line through the IO expander (`EXIO2`) — like the
  LCD reset, it isn't wired to a direct GPIO.
- Watches the controller's interrupt pin (GPIO 16, active-low/falling-edge) —
  before issuing an I2C read, the driver checks this pin and skips the
  transaction if it's high, because the chip can be asleep and will NACK I2C
  reads in that state.
- Decodes up to **2 simultaneous touch points** from the `TOUCH_POSITION`
  register block (each point: 4-bit + 8-bit X, 4-bit + 8-bit Y → 12-bit
  coordinates, plus 2 reserved bytes).
- Implements the generic `esp_lcd_touch_*` interface (coordinate
  swap/mirror, sleep/wake, teardown) so it's a drop-in for LVGL's touch input.

## Public API

### `CST820.h`
| Function | Description |
|---|---|
| `esp_err_t esp_lcd_touch_new_i2c_cst820(io, config, *tp)` | Creates a CST820 touch driver instance bound to an I2C panel-IO handle. |
| `void Touch_Init(void)` | High-level wrapper: configures I2C IO, resets the controller via `EXIO2`, registers the interrupt, and creates the driver instance. Call once at startup, after `I2C_Init()` / `EXIO_Init()`. |

### `esp_lcd_touch.h` (generic abstraction, implemented by this driver)
| Function | Description |
|---|---|
| `esp_err_t esp_lcd_touch_read_data(tp)` | Reads raw touch data from the controller. |
| `bool esp_lcd_touch_get_coordinates(tp, *x, *y, *strength, *point_num, max_point_num)` | Extracts coordinates, applying any configured swap/mirror transforms. |
| `esp_err_t esp_lcd_touch_set_swap_xy/set_mirror_x/set_mirror_y(tp, bool)` | Runtime coordinate transforms. |
| `esp_err_t esp_lcd_touch_enter_sleep/exit_sleep(tp)` | Power management. |
| `esp_err_t esp_lcd_touch_del(tp)` | Frees driver resources. |

## Hardware connections

| Signal | Pin |
|---|---|
| I2C (shared bus) | GPIO 7 (SCL) / GPIO 15 (SDA), 400 kHz |
| Interrupt | GPIO 16 (active-low, falling-edge) |
| Reset | via IO expander `EXIO2` |

## Settings / configurable values

| Setting | Value | Meaning |
|---|---|---|
| `ESP_LCD_TOUCH_IO_I2C_CST820_ADDRESS` | `0x15` | I2C address |
| `I2C_Touch_INT_IO` | `16` | Interrupt GPIO |
| `I2C_Touch_RST_IO` | `-1` | Not used directly — reset goes through `EXIO2` |
| `CONFIG_ESP_LCD_TOUCH_MAX_POINTS` | `2` | Max simultaneous touches reported |
| `x_max` / `y_max` (in `esp_lcd_touch_config_t`) | `480` / `480` | Used for mirroring math — should match the panel resolution |
| `flags.swap_xy` / `mirror_x` / `mirror_y` | all `0` | Coordinate orientation — flip these if the touch axes don't match the display orientation |

## Notes

- **Sleep/NACK handling:** the CST820 can go to sleep and refuse I2C
  communication; the driver checks the INT pin first and tolerates NACKs rather
  than treating them as fatal errors.
- **Reset sequence:** `EXIO2` is pulled low for ~10 ms, then high for ~50 ms,
  matching the controller's documented reset timing.
- `strength` is hardcoded to `50` for every reported point — the CST820 doesn't
  report real pressure/contact-area data, so don't rely on this value for
  anything beyond "a touch is present."
- A `portMUX` spinlock protects the shared touch-data struct between the
  interrupt context and the LVGL read callback.
- If your touch input feels mirrored or rotated relative to the display, adjust
  `swap_xy`/`mirror_x`/`mirror_y` here rather than in LVGL.
