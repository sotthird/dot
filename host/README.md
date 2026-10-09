# dot-host

The desktop half of dot: it polls a data source and streams it to the display over USB
serial. Each app here pairs with the firmware app of the same name.

## Install

```bash
cd host
python3 -m venv .venv && source .venv/bin/activate
pip install -e .                  # core (pyserial)
pip install -e ".[<app>]"         # plus the extras an app needs, if it lists any
```

Each app's README, in `firmware/main/apps/<name>/`, says what it needs and how to set up its data
source.

## Run

```bash
dot-host                          # every app whose extras are installed
dot-host ci cpu                   # or just these; python -m dot_host works too
dot-host --port /dev/ttyACM1
```

The device reports which app is on screen and the host polls only that one; the others are on
standby until you swipe to them. Name the apps the firmware was built with. `--port` defaults to
`/dev/ttyACM0` and `--baud` to `115200`. Only one process can hold the port, so close
`idf.py monitor` first. Stop with `Ctrl+C`. Apps may add options of their own, which are
described in their READMEs.

The host polls once a second (`POLL_INTERVAL` in `dot_host/cli.py`). An app that talks to a
rate-limited service should throttle itself inside `poll()`.

## Writing an app

Subclass `App` ([`dot_host/apps/base.py`](dot_host/apps/base.py)), add it to `_APPS` in
[`dot_host/apps/__init__.py`](dot_host/apps/__init__.py), and write the matching
firmware app. Any existing module in `dot_host/apps/` makes a good starting point.

- `start()`: one-time setup, for every app at startup.
- `on_activate()`: the app came on screen. Apps that only send on change should send again.
- `poll()`: returns the messages to send now: protocol lines (`str`) or binary payloads (`bytes`).
- `on_command(cmd)`: handles a `CMD:<cmd>` line sent back by the device.

## Development

```bash
pip install -e ".[dev,cpu,spotify]"
pytest
ruff check . && ruff format .
```

Helper scripts live in `scripts/` and are described in the README of the app they serve.
