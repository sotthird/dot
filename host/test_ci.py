#!/usr/bin/env python3
"""Inject fake CI status lines over serial to exercise the orb UI without real CI.

Usage:
    python test_ci.py [PORT] [BAUD] [--loop]

Defaults: PORT=/dev/ttyACM0, BAUD=115200. Add --loop to cycle forever.
Close any host app / `idf.py monitor` first — only one process can hold the port.
"""

import sys
import time

import serial

PORT = "/dev/ttyACM0"
BAUD = 115200

# (state, repo, title, wf_branch, runinfo)
STATES = [
    (
        "running",
        "sotthird/dot",
        "add ci orb redesign with gradient ring",
        "Build & Test / main",
        "#129  40s",
    ),
    ("success", "sotthird/dot", "fix: cache key for tests", "Build & Test / main", "#128  2m ago"),
    ("failure", "sotthird/dot", "broken pipeline step", "Build & Test / main", "#130  just now"),
    ("unknown", "sotthird/dot", "", "No runs", ""),
]

HOLD = 5  # seconds to hold each state


def main() -> None:
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    loop = "--loop" in sys.argv
    port = args[0] if len(args) > 0 else PORT
    baud = int(args[1]) if len(args) > 1 else BAUD

    s = serial.Serial(port, baud)
    print(f"Sending CI test lines to {port} @ {baud} (Ctrl+C to stop)")
    try:
        while True:
            for state, repo, title, wf, info in STATES:
                line = f"CI:{state}|{repo}|{title}|{wf}|{info}\n"
                print(f"  -> {state:8} {info}")
                s.write(line.encode())
                time.sleep(HOLD)
            if not loop:
                break
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        s.close()


if __name__ == "__main__":
    main()
