# dot

A desktop companion display. An ESP32-S3 touchscreen shows one glanceable screen, such as your CI
status, what's playing on Spotify, or CPU load, fed over USB serial by a Python program on your
computer.

Each screen is an **app** with a half on each side of the cable, so adding one never touches the
framework.

**Hardware**, drawn with [LVGL 9](https://lvgl.io/). Choose the board in `idf.py menuconfig` →
*Dot: Board*:

- [Waveshare ESP32-S3-Touch-LCD-2.1](https://www.waveshare.com/esp32-s3-touch-lcd-2.1.htm), a
  480×480 round display with touch, 16 MB flash and octal PSRAM (the default).
- [Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm),
  a 480×480 AMOLED with touch that turns its picture to follow how it is held, and dims when idle.

Pins and details: [`firmware/components/board`](firmware/components/board/README.md).

## Apps

| App | Shows | Host needs |
|---|---|---|
| `ci` | The whole display as a status light: green passing, amber and pulsing while running, red on failure. Tap to refresh. | [`gh` CLI](https://cli.github.com/) |
| `spotify` | Track, artist, album art with a matching accent color, progress, and prev/play/next buttons. | Spotify developer app |
| `cpu` | A radial gauge of host CPU usage. | `psutil` |

One app owns the screen at a time. Choose it in the firmware build and run the same one on the host.

## Quick start

**1. Flash the firmware.** Needs [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html)
v6.0 or newer, active in your shell.

```bash
cd firmware
idf.py set-target esp32s3                    # first time only
idf.py menuconfig                            # Dot: Board picks the hardware, Dot: App the app (default: ci)
idf.py -p /dev/ttyACM0 build flash monitor   # exit the monitor with Ctrl-]
```

**2. Run the host.**

```bash
cd host
python3 -m venv .venv && source .venv/bin/activate
pip install -e ".[spotify]"                  # or [cpu]; ci needs no extras
dot-host spotify --port /dev/ttyACM0
```

Per-app setup (GitHub auth, Spotify credentials) is in [`host/README.md`](host/README.md).
Only one process can hold the serial port, so close the monitor before starting the host.

## How it works

```
 host (Python)                                   device (ESP-IDF)
┌──────────────────────┐   USB serial   ┌─────────────────────────────┐
│ App.poll()           │ ─ "CI:..."  ─► │ serial_link → app.parse()   │
│                      │                │   app.update() → LVGL       │
│ App.on_command()     │ ◄─ "CMD:..." ─ │ touch → app_send_cmd()      │
└──────────────────────┘                └─────────────────────────────┘
```

The contract is newline-terminated text:

| Direction | Line | Meaning |
|---|---|---|
| host → device | `CI:<state>\|<repo>\|<title>\|<workflow / branch>\|<run info>` | CI status |
| host → device | `SPOTIFY:<track>\|<artist>\|<playing 0/1>\|<progress_ms>\|<duration_ms>` | now playing |
| host → device | `CPU:<percent>` | CPU load |
| host → device | `IMG:<w>x<h>:<bytes>:<rrggbb>` then `<bytes>` raw RGB565 | image and accent color |
| device → host | `CMD:<name>` | a touch action, e.g. `ci_refresh`, `play_pause` |

The firmware routes each line to the app whose `prefix` it starts with. `parse()` stashes the data
under a lock; the UI task then calls `update()` to render it.

## Layout

```
firmware/                    ESP-IDF project
├── components/board/        everything board-specific, behind board_init()
│   ├── lcd_2_1/             ST7701S RGB panel, CST820 touch, IO expander
│   ├── lcd_2_16/            CO5300 QSPI AMOLED, CST9217 touch, QMI8658 accelerometer
│   └── lvgl_port.c          LVGL display, input and tick
└── main/
    ├── main.c               startup
    ├── app.c, app.h         app registry and dispatch
    ├── serial_link.c        framing for the serial protocol
    └── apps/<name>/         <name>.c (data) and <name>_ui.c (LVGL screen)

host/                        Python package `dot_host`
├── dot_host/cli.py          dot-host entry point and poll loop
├── dot_host/serial_link.py  serial port and command listener
├── dot_host/apps/           ci.py, cpu.py, spotify.py (each with its data source)
├── scripts/                 fake_ci_status.py: drive the CI orb without real CI
└── tests/
```

## Adding an app

1. **Firmware:** create `firmware/main/apps/<name>/` with an `app_t` (the smallest example is
   [`apps/cpu`](firmware/main/apps/cpu/cpu.c)). Add its sources to `main/CMakeLists.txt`, a choice to
   `main/Kconfig.projbuild` and a branch to `main/main.c`.
2. **Host:** add `dot_host/apps/<name>.py` with an `App` subclass and list it in
   `dot_host/apps/__init__.py` ([details](host/README.md#writing-an-app)).

## Development

Python tests and linting are covered in [`host/README.md`](host/README.md#development). C is formatted
with [clang-format](.clang-format) (Google base, 4 spaces, 100 columns), pinned to the version in
`.pre-commit-config.yaml`:

```bash
find firmware -name '*.[ch]' -not -path '*/build/*' -not -path '*/managed_components/*' | xargs clang-format -i
```

`scripts/setup-hooks.sh` installs the pre-commit hooks (formatting, secret scan, conventional commits).

## Troubleshooting

- **Blank display:** watch `monitor` for errors during `board_init`. Check that *Dot: Board*
  matches your hardware.
- **AMOLED picture sideways or mirrored, or touch off:** the scan direction is the `0x36` entry in
  `lcd_2_16/board_amoled_2_16.c` and the touch flags are in `touch_init()` there.
- **AMOLED never rotates:** the log should say `qmi8658: accelerometer ready`. If it says the
  accelerometer was not found, the I2C address in `lcd_2_16/qmi8658.c` is wrong for your board.
- **No data on screen:** the host must run the same app the firmware was built with, on the right port.
- **`Resource busy` on the port:** another process, often `idf.py monitor`, has it open.
- **Spotify HTTP 429:** the host already honors `Retry-After`; raise `POLL_INTERVAL` in `dot_host/cli.py`.

## License

Apache 2.0 ([`LICENSE-APACHE`](LICENSE-APACHE)). The board code in `firmware/components/board/` derives
from Waveshare's and Espressif's Apache-licensed demo code; see [`NOTICE`](NOTICE).
