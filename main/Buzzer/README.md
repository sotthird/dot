# Buzzer

Thin on/off wrapper around the piezo buzzer, which is wired to the **TCA9554 IO
expander** (see [`EXIO`](../EXIO/README.md)) rather than directly to a GPIO pin.

## What it does

The buzzer's enable line is connected to expander pin `TCA9554_EXIO8`. This driver
just toggles that pin high/low through `Set_EXIO()`.

## Public API

| Function | Description |
|---|---|
| `void Buzzer_On(void)` | Drives `EXIO8` high — turns the buzzer on. |
| `void Buzzer_Off(void)` | Drives `EXIO8` low — turns the buzzer off. |

## Settings / configurable values

There is nothing to configure — the only "setting" is which expander pin the
buzzer is wired to (`TCA9554_EXIO8`, defined in `TCA9554PWR.h`). The buzzer is
active-high (logic `1` = sound on).

## Notes

- Requires `EXIO_Init()` to have run first so the expander pin is configured as an
  output (this is done as part of the general `EXIO_Init()` sequence, which also
  turns the buzzer off at boot).
- This is a simple on/off driver — there's no PWM/tone generation here. If you need
  variable-pitch tones you'd have to drive the buzzer from a PWM-capable GPIO
  instead of the IO expander.
