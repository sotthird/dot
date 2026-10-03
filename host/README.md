# dot-host

The desktop half of dot: it polls a data source and streams it to the display over USB
serial. Each app here pairs with the firmware app of the same name.

## Install

```bash
cd host
python3 -m venv .venv && source .venv/bin/activate
pip install -e .                  # core (pyserial)
pip install -e ".[cpu]"           # + psutil, for the CPU gauge
pip install -e ".[spotify]"       # + spotipy, pillow, ..., for Now Playing
```

The CI orb needs no Python extras, only the [`gh` CLI](https://cli.github.com/).

## Run

```bash
dot-host ci                       # or: python -m dot_host ci
dot-host spotify --port /dev/ttyACM1
```

The app must match the one the firmware was built with. `--port` defaults to
`/dev/ttyACM0` and `--baud` to `115200`. Only one process can hold the port, so close
`idf.py monitor` first. Stop with `Ctrl+C`.

## Apps

| App | Needs | Setup |
|---|---|---|
| `ci` | `gh` CLI | `gh auth login`, then run from inside the repo to watch, or pass `--repo PATH` / set `CI_REPO_DIR` |
| `cpu` | `[cpu]` extra | none |
| `spotify` | `[spotify]` extra | a [Spotify developer app](https://developer.spotify.com/dashboard); `cp .env.example .env` and fill it in |

`.env` is git-ignored. On first run Spotipy opens a browser to authorize; the token is
cached in `.cache` (also git-ignored). If you hit HTTP 429s, raise `POLL_INTERVAL` in
`dot_host/cli.py`.

## Writing an app

Subclass `App` ([`dot_host/apps/base.py`](dot_host/apps/base.py)), add it to `_APPS` in
[`dot_host/apps/__init__.py`](dot_host/apps/__init__.py), and write the matching
firmware app. [`apps/cpu.py`](dot_host/apps/cpu.py) is the smallest example.

- `start()`: one-time setup.
- `poll()`: returns the messages to send now: protocol lines (`str`) or binary payloads (`bytes`).
- `on_command(cmd)`: handles a `CMD:<cmd>` line sent back by the device.

## Development

```bash
pip install -e ".[dev,cpu,spotify]"
pytest
ruff check . && ruff format .
python scripts/fake_ci_status.py  # drive the CI orb with fake states, no CI needed
```
