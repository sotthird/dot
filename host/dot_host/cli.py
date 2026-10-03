import argparse
import sys
import time
from collections.abc import Sequence

import serial

from .apps import APP_NAMES, create
from .serial_link import SerialLink

POLL_INTERVAL = 1.0  # seconds between poll cycles


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="dot-host", description="Feed one app's data to the dot display."
    )
    parser.add_argument(
        "app",
        nargs="?",
        default="ci",
        choices=APP_NAMES,
        help="app to run; must match the firmware's app (default: ci)",
    )
    parser.add_argument("-p", "--port", default="/dev/ttyACM0", help="serial port")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    parser.add_argument(
        "--repo",
        help="ci: repository to watch (default: $CI_REPO_DIR, else the current directory)",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> None:
    args = parse_args(argv)
    options = {"repo_dir": args.repo} if args.app == "ci" else {}
    app = create(args.app, **options)

    try:
        link = SerialLink(args.port, args.baud)
    except serial.SerialException as e:
        sys.exit(f"Error: {e}")
    print(f"Connected to {args.port} at {args.baud} baud.")

    app.start()
    link.listen(app.on_command)
    print(f"Running the {args.app} app. Ctrl+C to stop.")

    try:
        while True:
            started = time.monotonic()
            for message in app.poll():
                link.send(message)
            time.sleep(max(0.0, POLL_INTERVAL - (time.monotonic() - started)))
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        link.close()
