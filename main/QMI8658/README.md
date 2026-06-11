# QMI8658

Driver for the **QMI8658** 6-axis IMU (3-axis accelerometer + 3-axis gyroscope),
with configurable measurement ranges, output data rates, and low-pass filtering.

## What it does

Communicates over the shared I2C bus at address `0x6B` (or `0x6A` if the
`SD0/SA0` strap is high). Provides setters for sensor configuration, raw register
access, and high-level "give me calibrated readings in physical units" functions.

## Data structure

```c
typedef struct __IMUdata {
    float x, y, z;
} IMUdata;
```

Globals `Accel` and `Gyro` hold the most recent calibrated readings (in **g** and
**deg/s** respectively).

## Public API

| Function | Description |
|---|---|
| `void QMI8658_Init(void)` | Configures the sensor with default ranges/rates/filters and computes calibration scale factors. Call once at startup. |
| `void QMI8658_Loop(void)` | Reads and updates `Accel` (calls `getAccelerometer`). Intended to be polled. |
| `void QMI8658_transmit/receive(...)` | Raw single-byte register write/read. |
| `void QMI8658_CTRL9_Write(uint8_t cmd)` | Sends a host command via `CTRL9` and waits for the chip to ACK it. |
| `void setAccODR/setGyroODR(...)` | Set accelerometer/gyroscope output data rate. |
| `void setAccScale/setGyroScale(...)` | Set accelerometer/gyroscope measurement range. |
| `void setAccLPF/setGyroLPF(...)` | Set accelerometer/gyroscope low-pass filter bandwidth. |
| `void setState(sensor_state_t)` | Switch between `default`, `running`, `power_down`, and `locking` states. |
| `void getRawReadings(int16_t *buf)` | Reads the raw 12-byte (6×16-bit) sensor block. |
| `float getAccX/Y/Z(void)`, `getGyroX/Y/Z(void)` | Return the latest calibrated values per axis. |
| `void getAccelerometer(void)` / `getGyroscope(void)` | Read raw data and apply the scale factor, updating `Accel`/`Gyro`. |

## Settings / configurable values

| Setting | Default | Options |
|---|---|---|
| Accelerometer range | `±4G` | `2G / 4G / 8G / 16G` (`acc_scale_t`) |
| Gyroscope range | `64 DPS` | `16…1024 DPS` (`gyro_scale_t`) |
| Accelerometer ODR | `8000 Hz` | `8000…30 Hz` normal, `128/21/11/3 Hz` low-power (`acc_odr_t`) |
| Gyroscope ODR | `8000 Hz` | `8000…30 Hz` (`gyro_odr_t`) |
| Accelerometer LPF | `LPF_MODE_0` (2.66% of ODR) | `LPF_MODE_0…3` (2.66% / 3.63% / 5.39% / 13.37% of ODR) |
| Gyroscope LPF | `LPF_MODE_3` (13.37% of ODR) | same `lpf_t` enum |
| `QMI8658_COMM_TIMEOUT` | `50 ms` | Timeout waiting for `CTRL9` command ACK |
| `QMI8658_REFRESH_DELAY` | `2000 µs` | Extra delay used in `locking` state for synchronized reads |

Each accelerometer range maps to a fixed LSB scale factor (e.g. `±2G → 0.00006103 g/LSB`,
`±16G → 0.0004883 g/LSB`) computed at init and applied when converting raw counts
to physical units.

## Register map (key registers)

| Register | Address | Purpose |
|---|---|---|
| `WHO_AM_I` / `REVISION_ID` | `0x00` / `0x01` | Device identification |
| `CTRL1`–`CTRL9` | `0x02`–`0x0A` | SPI mode, sensor enable, ODR/range, filters, power state, host commands |
| `STATUSINT` | `0x2D` | Status / interrupt flags |
| `TEMP_L/H` | `0x33`–`0x34` | Temperature |
| `AX/AY/AZ_L/H` | `0x35`–`0x3A` | Raw accelerometer data (little-endian, 16-bit signed) |
| `GX/GY/GZ_L/H` | `0x3B`–`0x40` | Raw gyroscope data (little-endian, 16-bit signed) |

## Notes

- **Sensor states:** `sensor_running` uses the high-speed clock for normal
  operation; `sensor_locking` additionally disables AHB clock gating (via `CTRL9`
  command `0x12`) so accelerometer and gyroscope samples are taken at the exact
  same instant — use this if you need synchronized data for orientation/fusion
  calculations, then restore normal gating afterwards.
- `CTRL1` bit 6 enables register **auto-increment**, which lets `getRawReadings`
  pull all 12 bytes (6 axes) in a single burst read.
- `CTRL7` values used: `0x43` = normal running, `0x83` = running with data
  locking, `0x00` = powered down.
- Raw values are little-endian (low byte then high byte) 16-bit signed integers —
  combine and sign-extend before applying the scale factor.
