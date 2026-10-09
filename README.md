# dot

A desktop companion display. An ESP32-S3 touchscreen shows one glanceable screen, fed over USB
serial by a Python program on your computer.

Each screen is an **app** with a half on each side of the cable, so adding one never touches the
framework.

## Hardware

Drawn with [LVGL 9](https://lvgl.io/). Choose the board in `idf.py menuconfig` → *Dot: Board*:

- [Waveshare ESP32-S3-Touch-LCD-2.1](https://www.waveshare.com/esp32-s3-touch-lcd-2.1.htm), a
  480×480 round display with touch, 16 MB flash and octal PSRAM (the default).
- [Waveshare ESP32-S3-Touch-AMOLED-2.16](https://www.waveshare.com/esp32-s3-touch-amoled-2.16.htm),
  a 480×480 AMOLED with touch that turns its picture to follow how it is held, and dims when idle.

Pins and per-board details: [`firmware/components/board`](firmware/components/board/README.md).

## Apps

Swipe left or right to move between apps; the screen fades through black, and dots down the left
edge show which one you are on. The AMOLED board also shows a status row at the bottom: USB cable, Wi-Fi, Bluetooth and the
battery level (with a bolt and green text while charging). An icon is green when connected, amber
while connecting or advertising, and grey when off.

Wi-Fi and Bluetooth LE only connect and show their state for now; the host link is still the USB
cable. Set the network under *Dot: Radios* in menuconfig (leave the name empty to keep Wi-Fi off).
With PlatformIO, put `CONFIG_DOT_WIFI_SSID="..."` and `CONFIG_DOT_WIFI_PASSWORD="..."` in
`firmware/boards/amoled_2_16.defaults`, and do not commit them. Bluetooth advertises as `dot`. Only the one on screen is running: the others are on
standby, and the host only fetches data for the active one. Choose which apps are built in under
*Dot: Apps* in menuconfig; they are shown in the order of `main/main.c`. Each app lives in
`firmware/main/apps/<name>/` and has its own README: what it shows, its host setup, its messages
and its touch actions.

## Quick start

**1. Flash the firmware.** Needs [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html)
v6.0 or newer, active in your shell.

```bash
cd firmware
idf.py set-target esp32s3                    # first time only
idf.py menuconfig                            # Dot: Board picks the hardware, Dot: Apps the apps
idf.py -p /dev/ttyACM0 build flash monitor   # exit the monitor with Ctrl-]
```

**PlatformIO instead of `idf.py`:** `firmware/platformio.ini` has an environment per board.

```bash
cd firmware
pio run -e amoled_2_16 -t upload     # or -e lcd_2_1; pio device monitor
```

The board and its settings come from `firmware/boards/<env>.defaults`, on top of `sdkconfig.defaults`.

**2. Run the host.** Set up each app's data source first, as described in its README.

```bash
cd host
python3 -m venv .venv && source .venv/bin/activate
pip install -e ".[<app>]"                    # extras an app needs are in its README
dot-host --port /dev/ttyACM0                 # all installed apps, or name some: dot-host ci cpu
```

Only one process can hold the serial port, so close the monitor before starting the host.
More host options: [`host/README.md`](host/README.md).

## How it works

```
 host (Python)                                   device (ESP-IDF)
┌──────────────────────┐   USB serial   ┌─────────────────────────────┐
│ App.poll()           │ ─ "<PFX>:.." ─►│ serial_link → app.parse()   │
│                      │                │   app.update() → LVGL       │
│ App.on_command()     │ ◄─ "CMD:..." ─ │ touch → app_send_cmd()      │
└──────────────────────┘                └─────────────────────────────┘
```

The two halves of an app share a **prefix** and speak newline-terminated text:

| Direction | Line | Meaning |
|---|---|---|
| host → device | `<PREFIX>:<payload>` | app data. The format is the app's own. |
| host → device | `IMG:<w>x<h>:<bytes>:<rrggbb>` then `<bytes>` of raw RGB565 | an image and an accent color |
| device → host | `CMD:<name>` | a touch action, named by the app |
| device → host | `APP:<name>` | the app now on screen, sent on every swipe |
| host → device | `HELLO` | asks for an `APP:` line; the host sends it until answered |

The firmware routes each line to the active app if it starts with that app's `prefix`, and drops
the rest. The host polls only the app the device reports. `parse()` runs on the
serial task and stashes the data under a lock; the UI task then calls `update()` to render it, so
LVGL is only ever touched from one task. On the host, an app produces messages from `poll()` and
handles commands in `on_command()`.

The board layer hides the hardware: `board_init()` brings up the display, touch and LVGL, and
`board_set_brightness()` sets the backlight. Apps never see which board they run on.

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
    └── apps/<name>/         <name>.c (data), <name>_ui.c (LVGL screen), README.md

host/                        Python package `dot_host`
├── dot_host/cli.py          dot-host entry point and poll loop
├── dot_host/serial_link.py  serial port and command listener
├── dot_host/apps/           one module per app, with its data source
├── scripts/                 development helpers
└── tests/
```

## Adding an app

1. **Firmware:** create `firmware/main/apps/<name>/` with an `app_t` (copy an existing app as a
   starting point). Its `name` is the host's name for it. Add its sources to `main/CMakeLists.txt`,
   an option to `main/Kconfig.projbuild` and an `app_register()` to `main/main.c`. Build the UI on
   the screen passed to `create_ui()`, and if the app animates by itself, stop that in `suspend()`
   and restart it in `resume()`.
2. **Host:** add `dot_host/apps/<name>.py` with an `App` subclass and list it in
   `dot_host/apps/__init__.py` ([details](host/README.md#writing-an-app)).
3. **Docs:** add a `README.md` next to the firmware app, following the existing ones: what it shows,
   host setup, protocol, files.

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
- **No data on screen:** the host must run the app that is on screen (`dot-host` with no arguments
  runs them all), on the right port.
- **An app is missing from the swipe:** it is off in *Dot: Apps*. A `sdkconfig` made before apps
  could be combined has them off; enable them in `idf.py menuconfig`.
- **`Resource busy` on the port:** another process, often `idf.py monitor`, has it open.
- **AMOLED picture sideways or mirrored, or touch off:** the scan direction is the `0x36` entry in
  `lcd_2_16/board_amoled_2_16.c` and the touch flags are in `touch_init()` there.
- **AMOLED never rotates:** the log should say `qmi8658: accelerometer ready`. If it says the
  accelerometer was not found, the I2C address in `lcd_2_16/qmi8658.c` is wrong for your board.

App-specific problems are in each app's README.

## License

Apache 2.0 ([`LICENSE-APACHE`](LICENSE-APACHE)). The board code in `firmware/components/board/` derives
from Waveshare's and Espressif's Apache-licensed demo code; see [`NOTICE`](NOTICE).
