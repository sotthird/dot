import argparse
import sys
import time
from collections.abc import Sequence

import serial

from .apps import APP_NAMES, App, AppUnavailable, create
from .runner import Runner
from .serial_link import SerialLink

POLL_INTERVAL = 1.0  # seconds between poll cycles


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="dot-host", description="Feed the apps on the dot display with data."
    )
    parser.add_argument(
        "apps",
        nargs="*",
        metavar="app",
        help=f"apps to run, from: {', '.join(APP_NAMES)} (default: all that are installed)",
    )
    parser.add_argument("-p", "--port", default="/dev/ttyACM0", help="serial port")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    parser.add_argument(
        "--repo",
        help="ci: repository to watch (default: $CI_REPO_DIR, else the current directory)",
    )
    args = parser.parse_args(argv)
    unknown = [name for name in args.apps if name not in APP_NAMES]
    if unknown:
        parser.error(f"unknown app {unknown[0]!r} (choose from {', '.join(APP_NAMES)})")
    return args


def create_apps(args: argparse.Namespace) -> dict[str, App]:
    """The apps asked for. With none named, every app whose packages are installed."""
    apps: dict[str, App] = {}
    for name in args.apps or APP_NAMES:
        options = {"repo_dir": args.repo} if name == "ci" else {}
        try:
            apps[name] = create(name, **options)
        except AppUnavailable as e:
            if args.apps:
                sys.exit(str(e))
            print(f"Skipping {name}: {e}")
    if not apps:
        sys.exit("No apps to run.")
    return apps


def main(argv: Sequence[str] | None = None) -> None:
    args = parse_args(argv)
    apps = create_apps(args)
    runner = Runner(apps)

    try:
        link = SerialLink(args.port, args.baud)
    except serial.SerialException as e:
        sys.exit(f"Error: {e}")
    print(f"Connected to {args.port} at {args.baud} baud.")

    for app in apps.values():
        app.start()
    link.listen(runner.on_command, runner.device_showing)
    print(f"Running {', '.join(apps)}; the one on the device's screen is active. Ctrl+C to stop.")

    try:
        while True:
            started = time.monotonic()
            if not runner.known:
                link.send("HELLO\n")  # ask the device which app is on screen
            for message in runner.poll():
                link.send(message)
            runner.wait(max(0.0, POLL_INTERVAL - (time.monotonic() - started)))
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        link.close()
