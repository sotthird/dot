| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# ESP32-S3 Touch LCD 2.1 — Modular Display Framework

A small **app framework** that turns the
[Waveshare ESP32-S3-Touch-LCD-2.1](https://www.waveshare.com/esp32-s3-touch-lcd-2.1.htm)
— a 480×480 circular touchscreen (ST7701S RGB panel + CST820 capacitive touch,
driven by [LVGL v9](https://lvgl.io/)) — into a desktop companion display.

The framework is the point. A Python **host** and an ESP-IDF **firmware** talk
over USB serial through one tiny, line-based contract; on top of that, each
"screen" is a self-contained, swappable **app** that plugs into a shared
init/UI/parse/update lifecycle. Adding a new display is writing one host module
and one firmware module — the core never changes.

Three example apps ship with it (all modular, all disabled-or-default by a
single registration line):

- **Spotify "Now Playing"** — track/artist, album art with color-themed UI, a
  progress bar, and on-screen previous/play-pause/next controls.
- **CPU gauge** — a radial arc of host CPU usage.
- **CI status orb** — the whole circle as a breathing light: green when GitHub
  Actions are passing, amber/pulsing while a run is in progress, red on failure.

See [Examples](#examples) for what each one does and how to enable it.

## How it works

The framework is just two halves agreeing on a line-based serial protocol — a
`PREFIX:` the firmware dispatches on, and a `CMD:` format for messages sent back.
Everything else (Spotify, CPU, CI…) is an app built on top.

```
 Host (Python, host/)                      ESP32-S3 (firmware, main/)
┌───────────────────────┐   USB serial    ┌───────────────────────────┐
│ App.poll() ───────────┼───"PREFIX:..."─►│ serial_reader_task        │
│   (one per app)       │                 │   → app_handle_line()     │
│                       │                 │     → <app>.parse()       │
│ App.on_command() ◄────┼───"CMD:..."─────┤     → <app>.update()      │
│                       │                 │       → LVGL screen       │
└───────────────────────┘                 └───────────────────────────┘
```

- Each host app's `poll()` returns a compact line (e.g.
  `SPOTIFY:<track>|<artist>|<is_playing>|<progress_ms>|<duration_ms>`); the host
  loop sends it over USB serial.
- The firmware's `serial_reader_task` routes each line by prefix to the matching
  app's `parse()`, which stores it behind a mutex; the LVGL task calls the app's
  `update()` to render it.
- On-screen touches send a `CMD:...` line back; the host hands it to every app's
  `on_command()`. (Binary payloads — e.g. album art — ride the same channel via
  an `IMG:` header; see `host/spotify_client.py` / `main/app.c`.)

## Project layout

```
main/                   ESP-IDF firmware
├── main.c              Hardware bring-up, task creation, app registration
├── app.h / app.c       App registry — the framework core (init/create_ui/parse/update)
├── apps/               Example app modules (spotify_app, cpu_app, ci_app) — one screen each
├── LVGL_UI/            LVGL screen builders used by the example apps
├── LVGL_Driver/        LVGL ↔ display/touch glue
└── <Driver folders>/   Vendor hardware drivers — each has its own README.md

host/                   Python host application
├── main.py             Entry point — registers apps, runs the poll loop
├── app.py              Base App class — the framework core (mirrors C-side app_t)
├── serial_conn.py      USB serial connection + command listener thread
├── apps/               Example app modules (spotify_app, cpu_app)
└── *_client.py /       Per-example helpers (spotify_client, cpu_monitor)
    *_monitor.py
```

> The CI status orb has no host side — it runs entirely on the device (WiFi +
> GitHub API). See [CI status orb](#ci-status-orb).

The `apps/` folder on each side is the modular **examples** collection — every
file in it is one self-contained screen you can enable, disable, copy, or delete
without touching the framework core (`app.*` / `main.c` registration).

Every vendor driver folder under `main/` (e.g. `LCD_Driver`, `Touch_Driver`,
`QMI8658`, …) has its own `README.md` explaining what the chip does, its pinout
and I2C/SPI addresses, and any tunable settings.

## The app framework

Both sides share the same plug-in shape; an app is one module on each:

- **Firmware side** (`main/apps/`): implement an `app_t` with `init`,
  `create_ui`, `parse`, and `update` callbacks and export it. See
  [`main/app.h`](main/app.h) and [`main/apps/cpu_app.c`](main/apps/cpu_app.c)
  for the smallest example.
- **Host side** (`host/apps/`): subclass `App` from [`host/app.py`](host/app.py)
  and implement `init`, `poll`, and (optionally) `on_command`. See
  [`host/apps/cpu_app.py`](host/apps/cpu_app.py).

Registering an app is one line on each side — `app_register(&your_app)` in
[`main/main.c`](main/main.c) and adding `YourApp()` to the `apps` list in
[`host/main.py`](host/main.py). Only one screen is active at a time (each app
loads its own LVGL screen), so the firmware registration selects which example
runs:

```c
app_register(&spotify_app);   // Spotify "Now Playing" (default)
// app_register(&cpu_app);    // CPU usage gauge
// app_register(&ci_app);     // CI / build status orb
```

## Examples

Each example is independent — pick one by registering it (firmware) and enabling
its host module. They share nothing but the framework contract.

### Spotify "Now Playing"

`main/apps/spotify_app.c` · `host/apps/spotify_app.py` · prefix `SPOTIFY:`

Shows the current track and artist, downloads the album art (converted to RGB565
on the host and streamed over the binary `IMG:` channel), extracts an accent
color from it to theme the UI, and renders a progress bar with elapsed/remaining
time. The on-screen previous/play-pause/next buttons send `CMD:` lines back to
the host, which drives the Spotify Web API.

- **Extra deps:** `spotipy`, `python-dotenv`, `pillow` (album art).
- **Credentials:** create a [Spotify Developer](https://developer.spotify.com/dashboard)
  app, then in `host/`:

  ```bash
  cp .env.example .env
  ```

  ```env
  SPOTIFY_CLIENT_ID=your_client_id
  SPOTIFY_CLIENT_SECRET=your_client_secret
  SPOTIFY_REDIRECT_URI=http://127.0.0.1:8080/callback
  ```

  `.env` is git-ignored — never commit real credentials. On the first run,
  Spotipy opens a browser to authorize the app; the token is cached locally
  (`.cache`, also git-ignored).

### CPU gauge

`main/apps/cpu_app.c` · `host/apps/cpu_app.py` · prefix `CPU:`

A radial arc showing host CPU usage, recolored from cool to hot as load rises.
The simplest example — a single value mapped to a single visual — and a good
starting point for your own app.

- **Extra deps:** `psutil`.

### CI status orb

`main/apps/ci_app.c` · prefix `CI:` · **runs entirely on-device — no host needed**

Turns the whole circular display into a single glanceable light for your latest
GitHub Actions run: **green** when passing, **amber and pulsing** while a run is
in progress, **red** on failure. Tap the orb to refresh immediately.

The firmware talks to the GitHub Actions REST API directly over WiFi, so there
is no Python host in the loop. Everything is configured **on the device**:

1. Register `ci_app` in `main/main.c` (the default) and flash.
2. On first boot the **settings screen** appears (or tap the **gear** button on
   the orb at any time). Enter:
   - **WiFi SSID** and **password**
   - **GitHub token** — a [personal access token](https://github.com/settings/tokens)
     with read access to Actions (fine-grained: *Actions → Read-only*; classic:
     `repo` scope for private repos, none needed for public)
   - **Repo** as `owner/name`
   - **Branch** to watch (leave blank to track the latest run on any branch)
3. Tap **Save**. The device connects, fetches status, and starts polling. The
   watched branch is shown on the main screen.

**Credential storage.** Settings are saved in encrypted NVS
(`CONFIG_NVS_ENCRYPTION`), with the keys held in a dedicated `nvs_keys`
partition. For the token to be genuinely protected at rest, also enable
**flash encryption** on the board — a one-time, **irreversible** eFuse burn:

```bash
idf.py menuconfig   # Security features → Enable flash encryption on boot
idf.py flash monitor
```

Do this only when you understand the consequences (read
[Espressif's flash encryption guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/flash-encryption.html)
first). Without it, NVS data is still encrypted but the keys sit in the
`nvs_keys` partition in plaintext.

## Hardware required

- [Waveshare ESP32-S3-Touch-LCD-2.1](https://www.waveshare.com/esp32-s3-touch-lcd-2.1.htm) — 480×480
  circular RGB display (ST7701S) with a CST820 capacitive touch panel and
  onboard sensors (IMU, RTC, battery ADC, IO expander, microSD), and
  **Octal PSRAM**
- A USB cable for power, programming, and the serial link to the host

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) v5.x+ (this project targets ESP-IDF v6 / LVGL v9.5)
- Python 3.10+ on the host machine
- Per-example extras (only what the app you enable needs) — see [Examples](#examples)

## Building and flashing the firmware

```bash
# from the project root
idf.py set-target esp32s3
idf.py -p /dev/ttyACM0 build flash monitor
```

The first build will take longer while ESP-IDF's component manager pulls the
managed components (including LVGL) into `managed_components/`.

(To exit the serial monitor, press `Ctrl-]`.) The default registration runs the
Spotify example; see [The app framework](#the-app-framework) to switch.

## Running the host application

The host app reads/writes the same USB serial port the firmware uses for
console output, so use the device node ESP-IDF prints when flashing (commonly
`/dev/ttyACM0` or `/dev/ttyUSB0`).

1. **Set up a virtual environment and install dependencies**

   ```bash
   cd host
   python3 -m venv .venv
   source .venv/bin/activate
   pip install pyserial               # framework core
   # plus whatever the enabled example needs, e.g.:
   pip install spotipy python-dotenv pillow   # Spotify
   pip install psutil                          # CPU gauge
   # (The CI orb needs no host — it's configured on the device.)
   ```

2. **Enable the example(s) you want** in `host/main.py` (uncomment them in the
   `apps` list) and configure them per [Examples](#examples).

3. **Run it**

   ```bash
   python main.py /dev/ttyACM0 115200
   ```

   Both arguments are optional — they default to `/dev/ttyACM0` and `115200`.
   You should see:

   ```
   Running 1 app(s). Ctrl+C to stop.
   ```

   The enabled app's data should appear on the display within a couple of
   seconds. Press `Ctrl+C` to stop.

## Code style

- C code is formatted with [clang-format](.clang-format) (Google base style, 4-space indent, 100-column limit):

  ```bash
  find main \( -name "*.c" -o -name "*.h" \) -exec clang-format -i {} +
  ```

- Python code is formatted/linted with [Ruff](host/pyproject.toml):

  ```bash
  cd host
  ruff format .
  ruff check .
  ```

## Troubleshooting

- **Display stays blank** — check the backlight level and pin in
  [`main/LCD_Driver/README.md`](main/LCD_Driver/README.md); confirm
  `idf.py monitor` shows `LCD_Init` completing without errors.
- **No data on screen** — the host isn't connected or isn't sending data yet;
  check that you're using the correct serial port, that the matching app is
  enabled on both sides, and that the firmware's `serial_reader_task` is running
  (visible in `idf.py monitor` output).
- **Serial port busy / `Resource busy`** — another process (often
  `idf.py monitor`) is holding the port; close it before starting the host app,
  or vice versa.
- **Spotify rate limiting (HTTP 429)** — the host already respects
  `Retry-After` headers and uses Spotipy's built-in retry/backoff; if you still
  see frequent 429s, increase `POLL_INTERVAL` in `host/main.py`.

For questions about the underlying hardware drivers, see the `README.md` in
each driver folder under `main/`.

## License

The driver code under `main/BAT_Driver/`, `main/Buzzer/`, `main/EXIO/`,
`main/I2C_Driver/`, `main/LCD_Driver/`, `main/LVGL_Driver/`, `main/PCF85063/`,
`main/QMI8658/`, `main/SD_Card/`, `main/Touch_Driver/`, and `main/Wireless/`
is derived from Waveshare's ESP32-S3-Touch-LCD-2.1 demo firmware and
Espressif's `esp_lcd_touch` component, both under the Apache License 2.0 (see
[`LICENSE-APACHE`](LICENSE-APACHE) and [`NOTICE`](NOTICE)). These files have
been modified for use in this project.

All other code (the app framework, `main/apps/`, `main/LVGL_UI/`, `host/`,
and project configuration) is original to this project.
