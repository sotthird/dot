"""``dot-host setup``: give the device its Wi-Fi network and Claude token over USB, once.

They are stored in the device's flash, not built into the firmware, so nothing secret ends up in
the repository or the build. The device restarts afterwards to join the network. The password and
the token are asked for without echo and are never printed.
"""

import argparse
import getpass
import shutil
import subprocess
import sys
import time
from collections.abc import Callable, Sequence

import serial

REPLY_TIMEOUT = 5.0  # seconds to wait for the device to acknowledge a setting
READY_TIMEOUT = 15.0  # seconds to wait for the firmware to come up after the port opens
LOG_SECONDS = 20  # with --debug, how long to show the device's own log after it restarts


class SetupError(Exception):
    pass


def mask(key: str, value: str) -> str:
    """What to show of a value on screen: the network name in full, secrets only by length."""
    if key == "wifi_ssid":
        return value
    return f"<{len(value)} characters>" if value else "<empty>"


def send_setting(
    ser: serial.Serial, key: str, value: str, say: Callable[[str], None] = print
) -> None:
    """Send one ``CFG:<key>|<value>`` line and wait for the device to say ``CFG:OK``."""
    if "\n" in value or "\r" in value or len(value) > 200:
        raise SetupError(f"{key}: value is too long or has a line break")
    say(f"-> CFG:{key}|{mask(key, value)}")
    ser.write(f"CFG:{key}|{value}\n".encode())
    ser.flush()
    deadline = time.monotonic() + REPLY_TIMEOUT
    while time.monotonic() < deadline:
        line = ser.readline().decode(errors="replace").strip()
        if line:
            say(f"<- {line}")
        if line == "CFG:OK":
            return
        if line == "CFG:ERR":
            raise SetupError(f"the device rejected {key}")
    raise SetupError(
        f"no answer for {key}: is the device on {ser.port} running firmware with setup support?"
    )


def wait_ready(ser: serial.Serial, say: Callable[[str], None] = print) -> None:
    """Opening the port can reset the board; say ``HELLO`` until the firmware answers ``APP:``."""
    say("Waiting for the device...")
    deadline = time.monotonic() + READY_TIMEOUT
    while time.monotonic() < deadline:
        ser.write(b"HELLO\n")
        ser.flush()
        while True:  # the boot log comes first; read until the line goes quiet
            line = ser.readline().decode(errors="replace")
            if line.startswith("APP:"):
                return
            if not line:
                break
    raise SetupError(
        f"the device on {ser.port} is not answering; is it running this firmware, "
        "and is nothing else using the port?"
    )


def apply(ser: serial.Serial, settings: dict[str, str], say: Callable[[str], None] = print) -> None:
    for key, value in settings.items():
        send_setting(ser, key, value, say)
    say("-> CFG:REBOOT")
    ser.write(b"CFG:REBOOT\n")
    ser.flush()


def show_log(ser: serial.Serial, seconds: float, say: Callable[[str], None] = print) -> None:
    """Print what the device says for a while after it restarts (Wi-Fi, usage fetches)."""
    say(f"Device log for {seconds:.0f}s (Ctrl+C to stop):")
    deadline = time.monotonic() + seconds
    try:
        while time.monotonic() < deadline:
            line = ser.readline().decode(errors="replace").rstrip()
            if line:
                say(f"   {line}")
    except (serial.SerialException, OSError):
        say("   (the port went away; the device is probably re-enumerating)")
    except KeyboardInterrupt:
        pass


def confirm(prompt: str, default: bool) -> bool:
    answer = input(f"{prompt} [{'Y/n' if default else 'y/N'}] ").strip().lower()
    return default if not answer else answer.startswith("y")


def get_token(read: Callable[[str], str] | None = None) -> str:
    """Ask whether the user already has a Claude token; only if not, help make one."""
    if not confirm("Do you already have a Claude token (sk-ant-oat01-...)?", False):
        if shutil.which("claude") and confirm("Run `claude setup-token` now to make one?", True):
            subprocess.run(["claude", "setup-token"], check=False)
            print("\nPaste the token it printed.")
        else:
            print(
                "Make one with `claude setup-token` on a computer where you are signed in to\n"
                "Claude Code, then run `dot-host setup` again, or paste it now."
            )
    return ask("Claude token (hidden, Enter to keep the current one): ", secret=True, read=read)


def ask(prompt: str, secret: bool = False, read: Callable[[str], str] | None = None) -> str:
    reader = read or (getpass.getpass if secret else input)
    return reader(prompt).strip()


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        prog="dot-host setup",
        description="Give the device its Wi-Fi and Claude token over USB. Blank answers keep "
        "what the device already has.",
    )
    parser.add_argument("-p", "--port", default="/dev/ttyACM0", help="serial port")
    parser.add_argument("-b", "--baud", type=int, default=115200)
    parser.add_argument("--ssid", help="Wi-Fi network name (asked for if left out)")
    parser.add_argument(
        "--debug",
        action="store_true",
        help=f"after saving, show the device's own log for {LOG_SECONDS}s",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> None:
    args = parse_args(argv)
    settings: dict[str, str] = {}

    ssid = args.ssid if args.ssid is not None else ask("Wi-Fi network name (Enter to skip): ")
    if ssid:
        settings["wifi_ssid"] = ssid
        settings["wifi_pass"] = ask(f"Password for {ssid} (hidden, empty if open): ", secret=True)
        if not settings["wifi_pass"]:
            print("Empty password: the network will be treated as open.")
    token = get_token()
    if token:
        settings["claude_tok"] = token
    else:
        print("No token entered: keeping the one on the device (if any).")
    if not settings:
        sys.exit("Nothing to set.")

    try:
        with serial.Serial(args.port, args.baud, timeout=1, exclusive=True) as ser:
            ser.dtr = False  # opening the port must not reset the board
            ser.rts = False
            wait_ready(ser)
            print(f"Connected to {args.port}.")
            apply(ser, settings)
            if args.debug:
                show_log(ser, LOG_SECONDS)
    except (serial.SerialException, SetupError) as e:
        hint = (
            "\nClose anything else using the port (e.g. `pio device monitor`, `dot-host`) and retry."
            if isinstance(e, serial.SerialException)
            else ""
        )
        sys.exit(f"Error: {e}{hint}")
    print("Saved. The device is restarting; Wi-Fi and usage should come up in a minute.")
