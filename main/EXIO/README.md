# EXIO (TCA9554PWR)

Driver for the **TCA9554** I2C 8-bit GPIO expander. The board uses it to control
several signals that would otherwise need dedicated GPIOs — LCD reset/CS, touch
reset, SD card D3, and the buzzer enable line.

## What it does

Talks to the TCA9554 over the shared I2C bus (see [`I2C_Driver`](../I2C_Driver/README.md))
at address `0x20`, and provides both low-level register access and higher-level
per-pin helpers (configure direction, read, write, toggle).

### Register map

| Register | Address | Purpose |
|---|---|---|
| Input port | `0x00` | Read-only — current logic level on each pin |
| Output port | `0x01` | Read/write — output level for pins configured as outputs |
| Polarity inversion | `0x02` | Inverts the sense of input pins |
| Configuration | `0x03` | Per-bit direction: `0` = output, `1` = input |

### Pin map

`TCA9554_EXIO1` … `TCA9554_EXIO8` map to bits `0x01`…`0x08`. On this board they're
wired to:

| Pin | Used for |
|---|---|
| EXIO1 | LCD reset |
| EXIO2 | Touch controller reset |
| EXIO3 | LCD SPI chip-select |
| EXIO4 | SD card D3 (power/pull-up enable) |
| EXIO8 | Buzzer enable |

## Public API

| Function | Description |
|---|---|
| `uint8_t Read_REG(uint8_t REG)` | Raw register read. |
| `void Write_REG(uint8_t REG, uint8_t Data)` | Raw register write. |
| `void Mode_EXIO(uint8_t Pin, uint8_t State)` | Set a single pin's direction (`0`=output, `1`=input). |
| `void Mode_EXIOS(uint8_t PinState)` | Set all 8 pins' directions at once via bitmask. |
| `uint8_t Read_EXIO(uint8_t Pin)` | Read a single pin's input level. |
| `uint8_t Read_EXIOS(void)` | Read all 8 pins at once. |
| `void Set_EXIO(uint8_t Pin, uint8_t State)` | Drive a single output pin high/low without touching the others. |
| `void Set_EXIOS(uint8_t PinState)` | Drive all 8 output pins via bitmask. |
| `void Set_Toggle(uint8_t Pin)` | Toggle a single pin's output state. |
| `void TCA9554PWR_Init(uint8_t PinState)` | Configure all 8 pins' direction in one call. |
| `esp_err_t EXIO_Init(void)` | Board-level setup: configures all pins as outputs and turns the buzzer off. Call this once at startup (after `I2C_Init()`). |

## Settings / configurable values

| Define | Value | Meaning |
|---|---|---|
| `TCA9554_ADDRESS` | `0x20` | I2C address of the expander |
| `TCA9554_INPUT_REG` | `0x00` | Input port register |
| `TCA9554_OUTPUT_REG` | `0x01` | Output port register |
| `TCA9554_Polarity_REG` | `0x02` | Polarity inversion register |
| `TCA9554_CONFIG_REG` | `0x03` | Direction/configuration register |
| `TCA9554_EXIO1`…`TCA9554_EXIO8` | `0x01`…`0x08` | Pin identifiers used by `Set_EXIO`/`Read_EXIO`/etc. |

## Notes

- `Set_EXIO()` validates that `Pin` is in `1..8` and `State` is `0` or `1` before
  writing.
- Must be initialized after `I2C_Init()` since all communication goes over I2C.
- Because the LCD reset, touch reset, LCD CS, and SD D3 lines all live behind this
  expander, `EXIO_Init()` effectively gates the rest of hardware bring-up — the
  display/touch/SD drivers call into this module to drive those lines.
