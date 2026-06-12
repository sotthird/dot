#!/usr/bin/env python3
"""Entry point — register apps here, nothing else changes."""

import sys
import time

from apps.spotify_app import SpotifyApp
from serial_conn import SerialConn

# from apps.cpu_app import CpuApp     # uncomment to enable

PORT = sys.argv[1] if len(sys.argv) > 1 else "/dev/ttyACM0"
BAUD = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
POLL_INTERVAL = 1  # seconds between poll cycles


def main():
    conn = SerialConn(PORT, BAUD)
    conn.connect()

    # ---- Register apps here ----------------------------------------
    apps = [
        SpotifyApp(),
        # CpuApp(),
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
