# I2C_Driver

Sets up the shared I2C master bus used by every I2C peripheral on the board (IO
expander, touch controller, RTC, IMU) and provides simple register read/write
helpers on top of ESP-IDF's new `i2c_master` API.

## What it does

- Initializes **I2C port 0** as master with internal pull-ups enabled and a glitch
  filter (7-cycle ignore count) for noise rejection.
- Exposes a global bus handle (`i2c_bus_handle`) that other drivers attach device
  handles to.
- Provides generic `I2C_Write`/`I2C_Read` helpers that perform a register-address
  + data transaction (write the register address first, then read/write the
  payload), which is the common pattern for the sensors on this board.

## Public API

| Function | Description |
|---|---|
| `void I2C_Init(void)` | Initializes the I2C master bus. Call once at startup, before any other I2C-based driver. |
| `esp_err_t I2C_Write(uint8_t Driver_addr, uint8_t Reg_addr, const uint8_t *Reg_data, uint32_t Length)` | Writes `Length` bytes to `Reg_addr` on device `Driver_addr`. |
| `esp_err_t I2C_Read(uint8_t Driver_addr, uint8_t Reg_addr, uint8_t *Reg_data, uint32_t Length)` | Reads `Length` bytes from `Reg_addr` on device `Driver_addr`. |

## Settings / configurable values

| Define | Value | Meaning |
|---|---|---|
| `I2C_Touch_SCL_IO` | `7` | SCL GPIO pin |
| `I2C_Touch_SDA_IO` | `15` | SDA GPIO pin |
| `I2C_MASTER_NUM` | `I2C_NUM_0` | I2C controller/port used |
| `I2C_MASTER_FREQ_HZ` | `400000` | Bus clock speed (400 kHz / Fast Mode) |
| `I2C_MASTER_TIMEOUT_MS` | `1000` | Per-transaction timeout |

## Notes

- Uses the **new ESP-IDF `i2c_master` driver** (not the legacy `driver/i2c.h` API),
  with 7-bit addressing.
- Device handles are created and removed on the fly per transaction rather than
  held persistently — simple and safe for a bus shared by several low-traffic
  peripherals, at a small overhead cost per call.
- All other I2C peripheral drivers (`EXIO`, `Touch_Driver`, `PCF85063`, `QMI8658`)
  depend on `i2c_bus_handle` being initialized first — always call `I2C_Init()`
  before them.
