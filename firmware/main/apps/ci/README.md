# ci

The whole display as a CI status light for the latest GitHub Actions run.

| State | Look |
|---|---|
| success | green |
| running | amber, with a pulsing ring and job progress (`2/5 build`) |
| failure | red |
| unknown | grey (no runs, or the status could not be fetched) |

Below the orb: the repo, the run title and the workflow / branch.

**Touch:** tap the orb to refresh now instead of waiting for the next poll.

## Host

Needs the [`gh` CLI](https://cli.github.com/), signed in with `gh auth login`. No Python extras.

```bash
dot-host ci                          # watches the repo in the current directory
dot-host ci --repo ~/code/my-project # or pass a path, or set CI_REPO_DIR
```

`gh` finds the repository from the working directory. The host asks it for the latest run every
5 seconds and only sends a message when something changed.

## Protocol

```
CI:<state>|<repo>|<title>|<workflow / branch>|<run info>
```

`state` is `running`, `success`, `failure` or `unknown`. Text is ASCII only and cannot contain `|`.
The device sends `CMD:ci_refresh` when the orb is tapped, which makes the host fetch and re-send
immediately. The host also handles `ci_open` (opens the run in a browser), but the current UI does
not send it.

## Files

- Firmware: [`ci.c`](ci.c) (parsing), [`ci_ui.c`](ci_ui.c) (the orb)
- Host: [`host/dot_host/apps/ci.py`](../../../../host/dot_host/apps/ci.py)

## Development

Drive the orb through its states without real CI:

```bash
python host/scripts/fake_ci_status.py [PORT] [BAUD] [--loop]
```

Close `idf.py monitor` and `dot-host` first; only one process can hold the serial port.
