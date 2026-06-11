# PCF85063

Driver for the **PCF85063** I2C real-time clock (NXP) — read/write the date and
time, configure alarms, and format timestamps as strings.

## What it does

Talks to the RTC over the shared I2C bus at address `0x51`. All time/date values
on the chip are stored in **BCD** (binary-coded decimal); the driver converts to
and from regular integers via `decToBcd()`/`bcdToDec()` and masks off control bits
on read (e.g. `0x7F` for seconds, `0x3F` for hours).

## Data structure

```c
typedef struct {
    uint16_t year;   // stored on-chip relative to a 1970 offset
    uint8_t  month;  // 1-12
    uint8_t  day;    // 1-31
    uint8_t  dotw;   // day of week, 0 = Sunday ... 6 = Saturday
    uint8_t  hour;   // 0-23
    uint8_t  minute; // 0-59
    uint8_t  second; // 0-59
} datetime_t;
```

## Public API

| Function | Description |
|---|---|
| `void PCF85063_Init(void)` | Applies the default control register settings. Call once at startup. |
| `void RTC_Loop(void)` | Reads the current time into the global `datetime`. Intended to be polled periodically. |
| `void PCF85063_Reset(void)` | Issues a software reset (`RTC_CTRL_1_SR`). |
| `void PCF85063_Set_Time(datetime_t)` / `PCF85063_Set_Date(datetime_t)` / `PCF85063_Set_All(datetime_t)` | Write time, date, or both. |
| `void PCF85063_Read_Time(datetime_t *)` | Reads the current date and time from the chip. |
| `void PCF85063_Enable_Alarm(void)` | Enables the alarm interrupt (`RTC_CTRL_2_AIE`). |
| `uint8_t PCF85063_Get_Alarm_Flag(void)` | Returns whether the alarm has fired (`RTC_CTRL_2_AF`). |
| `void PCF85063_Set_Alarm(datetime_t)` / `PCF85063_Read_Alarm(datetime_t *)` | Configure / read back the alarm time. |
| `void datetime_to_str(char *out, datetime_t)` | Formats a `datetime_t` as `" YYYY.MM.DD WD HH:MM:SS "`. |

## Register map / settings

| Register | Address | Purpose |
|---|---|---|
| `RTC_CTRL_1_ADDR` | `0x00` | STOP, software reset, 12/24 h, oscillator capacitor select |
| `RTC_CTRL_2_ADDR` | `0x01` | Alarm interrupt enable/flag, minute/half-minute interrupt, timer flag |
| Time/date registers | `0x04`–`0x0A` | second, minute, hour, day, weekday, month, year (BCD) |
| Alarm registers | `0x0B`–`0x0F` | second, minute, hour, day, weekday alarm values |
| Timer registers | `0x10`–`0x11` | countdown timer value/mode |

Useful flag bits:

| Flag | Value | Meaning |
|---|---|---|
| `RTC_CTRL_1_STOP` | `0x20` | Stop/start the RTC oscillator |
| `RTC_CTRL_1_SR` | `0x10` | Software reset |
| `RTC_CTRL_1_CAP_SEL` | `0x01` | Oscillator load capacitance select (7 pF vs 12.5 pF — must match the crystal on the board) |
| `RTC_CTRL_2_AIE` / `AF` | `0x80` / `0x40` | Alarm interrupt enable / alarm-fired flag |
| `RTC_TIMER_MODE_TE` | `0x04` | Countdown timer enable |
| `RTC_ALARM` | `0x80` | Set on an alarm field to disable matching on that field |

## Notes

- The default control value applied at init is `RTC_CTRL_1_DEFAULT | RTC_CTRL_1_CAP_SEL`
  — change `RTC_CTRL_1_CAP_SEL` only if your board uses a different crystal load
  capacitance than the one this value assumes.
- Stored `year` is offset from 1970 — `PCF85063_Read_Time`/`Set_Time` handle the
  conversion for you; don't add the offset yourself.
- Alarm fields can be individually disabled by setting their high bit (`RTC_ALARM`,
  `0x80`), so you can build "alarm every day at HH:MM" style triggers.
- Month name strings ("Jan"–"Dec") are hardcoded for convenience in formatting/logging.
