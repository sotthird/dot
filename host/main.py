#!/usr/bin/env python3
"""Entry point — register apps here, nothing else changes."""

import sys
import time

# from apps.cpu_app import CpuApp     # uncomment to enable
from apps.ci_app import CiApp  # uncomment to enable (needs `gh` CLI)
from serial_conn import SerialConn

POLL_INTERVAL = 1  # seconds between poll cycles


def parse_args(argv):
    """Pull out --ci-repo PATH (CI app target); the rest are positional PORT BAUD."""
    ci_repo = None
    positional = []
    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg in ("--ci-repo", "--repo"):
            ci_repo = argv[i + 1] if i + 1 < len(argv) else None
            i += 2
            continue
        if arg.startswith("--ci-repo="):
            ci_repo = arg.split("=", 1)[1]
        else:
            positional.append(arg)
        i += 1
    port = positional[0] if len(positional) > 0 else "/dev/ttyACM0"
    baud = int(positional[1]) if len(positional) > 1 else 115200
    return port, baud, ci_repo


def main():
    port, baud, ci_repo = parse_args(sys.argv[1:])

    conn = SerialConn(port, baud)
    conn.connect()

    # ---- Register apps here ----------------------------------------
    apps = [
        # SpotifyApp(),
        # CpuApp(),
        CiApp(repo_dir=ci_repo),  # --ci-repo PATH overrides; else CI_REPO_DIR env / cwd
    ]
    # ----------------------------------------------------------------

    clients = [app.init() for app in apps]

    def on_command(cmd: str):
        for app, client in zip(apps, clients):
            app.on_command(cmd, client)

    conn.start_command_listener(on_command)
    print(f"Running {len(apps)} app(s). Ctrl+C to stop.")

    try:
        while True:
            start = time.monotonic()

            for app, client in zip(apps, clients):
                msg = app.poll(client)
                if msg:
                    conn.send(msg)
                for blob in app.poll_binary(client):
                    conn.send_bytes(blob)

            remaining = POLL_INTERVAL - (time.monotonic() - start)
            if remaining > 0:
                time.sleep(remaining)

    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        conn.close()


if __name__ == "__main__":
    main()
