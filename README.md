| Supported Targets | ESP32-S3 |
| ----------------- | -------- |

# ESP32-S3 Touch LCD 2.1 — Now Playing Display

A 480×480 circular touchscreen (ST7701S RGB panel + CST820 capacitive touch,
driven by [LVGL v9](https://lvgl.io/)) turned into a desktop companion display.
A small Python host app talks to the board over USB serial and shows what's
currently playing on Spotify — track, artist, album-style "now playing" screen,
a progress bar with elapsed/remaining time, and physical on-screen
previous/play-pause/next controls that send commands back to the host.

The firmware is built around a small **app registry** so new "screens" can be
added without touching the core — each screen is a self-contained module that
plugs into the same init/UI/parse/update lifecycle (see
[`main/app.h`](main/app.h)). A CPU-usage gauge screen is included as a second
example app (disabled by default).

## How it works

```
 Host (Python, host/)                      ESP32-S3 (firmware, main/)
┌───────────────────────┐   USB serial    ┌───────────────────────────┐
│ spotify_client.py     │  "SPOTIFY:..."  │ serial_reader_task        │
│  (Spotipy / OAuth) ───┼────────────────►│   → app_handle_line()     │
│                       │                 │     → spotify_app.parse() │
│ apps/spotify_app.py   │   "CMD:..."     │                           │
│  ◄────────────────────┼─────────────────┤ spotify_app.send_cmd()    │
└───────────────────────┘                 │   → LVGL "Now Playing" UI │
                                           │   (buttons, progress bar) │
                                           └───────────────────────────┘
```

- The host polls the Spotify Web API (rate-limit aware) and pushes a compact
  line like `SPOTIFY:<track>|<artist>|<is_playing>|<progress_ms>|<duration_ms>`
  over USB serial.
- The firmware parses it, stores it behind a mutex, and the LVGL task renders
  it onto the "Now Playing" screen.
- Pressing a control button on the touchscreen sends `CMD:play_pause`,
  `CMD:next`, or `CMD:prev` back to the host, which forwards it to the Spotify
  Web API.

## Project layout

```
main/                   ESP-IDF firmware
├── main.c              Hardware bring-up, task creation, app registration
├── app.h / app.c       App registry (init / create_ui / parse / update dispatch)
├── apps/               Self-contained screen modules (spotify_app, cpu_app)
├── LVGL_UI/            LVGL screens (CPU gauge, Spotify "Now Playing")
├── LVGL_Driver/        LVGL ↔ display/touch glue
└── <Driver folders>/   Vendor hardware drivers — each has its own README.md

host/                   Python host application
├── main.py             Entry point — registers apps, runs the poll loop
├── app.py              Base App class (mirrors the C-side app_t interface)
├── serial_conn.py      USB serial connection + command listener thread
├── spotify_client.py   Spotify Web API wrapper (OAuth, rate-limit handling)
└── apps/               spotify_app.py, cpu_app.py
```

Every vendor driver folder under `main/` (e.g. `LCD_Driver`, `Touch_Driver`,
`QMI8658`, …) has its own `README.md` explaining what the chip does, its pinout
and I2C/SPI addresses, and any tunable settings.

## Hardware required

- The ESP32-S3 board this project targets — 480×480 circular RGB display
  (ST7701S) with a CST820 capacitive touch panel and onboard sensors (IMU, RTC,
  battery ADC, IO expander, microSD), and **Octal PSRAM**
- A USB cable for power, programming, and the serial link to the host

## Prerequisites

- [ESP-IDF](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/get-started/index.html) v5.x+ (this project targets ESP-IDF v6 / LVGL v9.5)
- Python 3.10+ on the host machine
- A [Spotify Developer](https://developer.spotify.com/dashboard) app (Client ID/Secret) with the redirect URI you'll configure below

## Building and flashing the firmware

```bash
# from the project root
idf.py set-target esp32s3
idf.py -p /dev/ttyACM0 build flash monitor
```

The first build will take longer while ESP-IDF's component manager pulls the
managed components (including LVGL) into `managed_components/`.

(To exit the serial monitor, press `Ctrl-]`.)

By default only the Spotify "Now Playing" screen is registered. To try the CPU
gauge screen instead/as well, edit `main/main.c`:

```c
app_register(&spotify_app);
app_register(&cpu_app);   // uncomment to enable
```

## Running the host application

The host app reads/writes the same USB serial port the firmware uses for
console output, so use the device node ESP-IDF prints when flashing (commonly
`/dev/ttyACM0` or `/dev/ttyUSB0`).

1. **Set up a virtual environment and install dependencies**

   ```bash
   cd host
   python3 -m venv .venv
   source .venv/bin/activate
   pip install spotipy pyserial python-dotenv
   ```

2. **Configure Spotify credentials**

   Copy the example env file and fill in your app's credentials from the
   [Spotify Developer Dashboard](https://developer.spotify.com/dashboard):

   ```bash
   cp .env.example .env
   ```

   ```env
   SPOTIFY_CLIENT_ID=your_client_id
   SPOTIFY_CLIENT_SECRET=your_client_secret
   SPOTIFY_REDIRECT_URI=http://127.0.0.1:8080/callback
   ```

   `.env` is git-ignored — never commit real credentials.

3. **Run it**

   ```bash
   python main.py /dev/ttyACM0 115200
   ```

   Both arguments are optional — they default to `/dev/ttyACM0` and `115200`.
   On the first run, Spotipy will open a browser window for you to authorize
   the app; the resulting token is cached locally (`.cache`, also git-ignored).

   You should see:

   ```
   Running 1 app(s). Ctrl+C to stop.
   ```

   Play something on Spotify and the track, artist, progress bar, and play/pause
   state should appear on the display within a couple of seconds. Tap the
   on-screen buttons to skip tracks or pause/resume playback.

   Press `Ctrl+C` to stop.

## Adding a new screen / app

Both sides share the same plug-in shape:

- **Firmware side** (`main/apps/`): implement an `app_t` with `init`,
  `create_ui`, `parse`, and `update` callbacks, register it in `main.c` via
  `app_register(&your_app)`. See [`main/app.h`](main/app.h) and
  [`main/apps/spotify_app.c`](main/apps/spotify_app.c) for the pattern.
- **Host side** (`host/apps/`): subclass `App` from [`host/app.py`](host/app.py)
  and implement `init`, `poll`, and `on_command`; register it in
  [`host/main.py`](host/main.py).

The two sides only need to agree on a line-based serial protocol — a `PREFIX:`
the firmware can dispatch on, and a `CMD:` format for commands sent back.

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
- **`---%` / no data on screen** — the host isn't connected or isn't sending
  data yet; check that you're using the correct serial port and that the
  firmware's `serial_reader_task` is running (visible in `idf.py monitor`
  output).
- **Serial port busy / `Resource busy`** — another process (often
  `idf.py monitor`) is holding the port; close it before starting the host app,
  or vice versa.
- **Spotify rate limiting (HTTP 429)** — the host already respects
  `Retry-After` headers and uses Spotipy's built-in retry/backoff; if you still
  see frequent 429s, increase `POLL_INTERVAL` in `host/main.py`.

For questions about the underlying hardware drivers, see the `README.md` in
each driver folder under `main/`.
