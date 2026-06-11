# SD_Card (SD_MMC)

Mounts a microSD card over the SDMMC peripheral in **1-line SPI/SD mode** and
exposes a FAT filesystem at `/sdcard`, plus simple file read/write helpers.

## What it does

- Drives the SD card's `D3` line through the IO expander (`EXIO4`) — on this
  board it isn't wired to a direct GPIO, so it must be enabled/stabilized before
  the card will respond (see Notes).
- Configures the SDMMC host/slot for **1-bit data width** (`D0` only; `D1`/`D2`
  unused) and mounts a FAT filesystem, auto-formatting if the mount fails.
- Provides minimal file write/read helpers built on the standard C `FILE` API.
- Also includes `Flash_Searching()`, a small helper that logs the size of the
  ESP32's internal flash (unrelated to the SD card itself, but bundled here).

## Public API

| Function | Description |
|---|---|
| `esp_err_t SD_Card_D3_EN(void)` / `SD_Card_D3_Dis(void)` | Enable/disable the SD card's `D3` line via `EXIO4`, with a settle delay. |
| `void SD_Init(void)` | Full bring-up: enables `D3`, configures the SDMMC host/slot, and mounts the FAT filesystem at `/sdcard`. Call once at startup, after `EXIO_Init()`. |
| `esp_err_t s_example_write_file(const char *path, char *data)` | Writes a string to a file. |
| `esp_err_t s_example_read_file(const char *path)` | Reads a file and logs its contents. |
| `void Flash_Searching(void)` | Logs the internal flash chip size. |

## Hardware connections

| Signal | Pin |
|---|---|
| CLK | GPIO 2 |
| CMD | GPIO 1 |
| D0 | GPIO 42 |
| D1 / D2 | not used (`-1`) |
| D3 | via IO expander `EXIO4` |

## Settings / configurable values

| Define | Value | Meaning |
|---|---|---|
| `CONFIG_EXAMPLE_PIN_CLK` / `_CMD` / `_D0` | `2` / `1` / `42` | SDMMC pin mapping |
| `CONFIG_EXAMPLE_PIN_D3` | `-1` | Not used directly — handled via `EXIO4` instead |
| `EXAMPLE_MAX_CHAR_SIZE` | `64` | Max buffer size for the file read/write helpers |
| `MOUNT_POINT` | `"/sdcard"` | Filesystem mount point |
| `format_if_mount_failed` | `true` | Auto-format the card if mounting the FAT filesystem fails |
| `max_files` | `5` | Max concurrently open files |
| `allocation_unit_size` | `16 KB` | FAT cluster size used when formatting |
| `SDMMC_HOST_DEFAULT()` frequency | `20 MHz` | Default bus speed (override `max_freq_khz` to change) |
| `SDMMC_SLOT_FLAG_INTERNAL_PULLUP` | enabled | Uses the chip's internal pull-ups (an external ~10k pull-up is still recommended) |

## Notes

- **Why `D3` goes through the IO expander:** the line isn't connected to a GPIO on
  this board; it has to be raised through `EXIO4` to power up / pull up the card
  before SPI/SD communication will succeed. `SD_Card_D3_EN()` adds a ~50 ms delay
  to let the card exit its power-on state and stabilize before use.
- Only **1-wire mode** is wired up (the 4-wire slot config is present in the code
  but commented out) — don't expect higher-throughput 4-bit transfers without
  rewiring.
- If mounting fails, double check pull-up resistors first — the driver logs a
  hint to that effect, since flaky pull-ups are the most common cause of mount
  failures on this kind of wiring.
