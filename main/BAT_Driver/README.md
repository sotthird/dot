# BAT_Driver

Reads the board's supply/battery voltage through the ESP32-S3's internal ADC and
exposes it as a calibrated float in volts.

## What it does

- Configures **ADC1 channel 3** (GPIO 4) in oneshot mode with **12 dB attenuation**
  (full 0–3.3 V range).
- Uses the ADC calibration scheme available on the chip — **Curve Fitting** if the
  eFuse calibration data is present, otherwise falls back to **Line Fitting** (and
  logs a warning if neither is available, continuing with raw readings).
- Converts the calibrated millivolt reading to volts and applies a fixed correction
  factor (`Measurement_offset`) to compensate for the board's voltage divider /
  measurement error.

## Public API

| Function | Description |
|---|---|
| `void BAT_Init(void)` | Initializes the ADC unit and calibration scheme. Call once at startup. |
| `float BAT_Get_Volts(void)` | Takes a single ADC reading and returns the battery voltage in volts. |

## Settings / configurable values

| Define | Value | Meaning |
|---|---|---|
| `EXAMPLE_ADC1_CHAN3` | `ADC_CHANNEL_3` (GPIO 4) | ADC input pin |
| `EXAMPLE_ADC_ATTEN` | `ADC_ATTEN_DB_12` | Input attenuation (sets the measurable voltage range) |
| `Measurement_offset` | `0.994500` | Empirical correction multiplier applied to the measured voltage |

Conversion formula used internally:

```
volts = (raw_millivolts * 3.0 / 1000.0) / Measurement_offset
```

## Notes

- `BAT_analogVolts` is a global that holds the most recent reading.
- Sampling is on-demand (oneshot), not continuous — call `BAT_Get_Volts()` whenever
  you need a fresh value.
- If the ADC eFuse calibration isn't burned, the driver still works but readings
  will be less accurate; this is logged at startup.
