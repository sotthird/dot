import sys
import threading

import serial


class SerialConn:
    def __init__(self, port="/dev/ttyACM0", baud=115200):
        self.port = port
        self.baud = baud
        self.ser = None

    def connect(self):
        try:
            self.ser = serial.Serial(self.port, self.baud, timeout=1)
            self.ser.dtr = False
            self.ser.rts = False
            print(f"Connected to {self.port} at {self.baud} baud.")
        except serial.SerialException as e:
            print(f"Error: {e}")
            sys.exit(1)

    def send(self, msg: str):
        self.ser.write(msg.encode())
        self.ser.flush()

    def send_bytes(self, data: bytes):
        self.ser.write(data)
        self.ser.flush()

    def start_command_listener(self, on_command):
        """Start a background thread that calls on_command(cmd) for each CMD: line."""

        def _reader():
            buf = b""
            while True:
                try:
                    chunk = self.ser.read(64)
                    if not chunk:
                        continue
                    buf += chunk
                    while b"\n" in buf:
                        line, buf = buf.split(b"\n", 1)
                        text = line.decode(errors="replace").strip()
                        if text.startswith("CMD:"):
                            cmd = text[4:]
                            on_command(cmd)
                except Exception:
                    break

        t = threading.Thread(target=_reader, daemon=True)
        t.start()

    def close(self):
        if self.ser:
            self.ser.close()
