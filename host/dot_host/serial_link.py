"""USB serial link to the device.

The protocol is newline-terminated text. The host sends lines starting with an app's
prefix (``CI:``, ``CPU:``, ``SPOTIFY:``) and binary image payloads announced by an
``IMG:`` header; the device answers with ``CMD:<name>`` lines when its UI is touched.
The device also says which app is on screen with ``APP:<name>``, when the user swipes to
another one and in reply to the host's ``HELLO``.
"""

import threading
from collections.abc import Callable

import serial


class SerialLink:
    def __init__(self, port: str, baud: int):
        self._ser = serial.Serial(port, baud, timeout=1)
        # Opening the port must not reset the board
        self._ser.dtr = False
        self._ser.rts = False

    def send(self, data: str | bytes) -> None:
        self._ser.write(data.encode() if isinstance(data, str) else data)
        self._ser.flush()

    def listen(self, on_command: Callable[[str], None], on_app: Callable[[str], None]) -> None:
        """From a background thread, call on_command(name) for every ``CMD:<name>`` line and
        on_app(name) for every ``APP:<name>`` line."""

        def reader() -> None:
            buf = b""
            while True:
                try:
                    chunk = self._ser.read(64)
                except (serial.SerialException, OSError):
                    return
                if not chunk:
                    continue
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    text = line.decode(errors="replace").strip()
                    if text.startswith("CMD:"):
                        on_command(text[4:])
                    elif text.startswith("APP:"):
                        on_app(text[4:])

        threading.Thread(target=reader, daemon=True).start()

    def close(self) -> None:
        self._ser.close()
