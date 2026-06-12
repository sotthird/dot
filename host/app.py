class App:
    """
    Base class for all display apps.

    Mirrors the C-side app_t interface:
      init()        → one-time setup, returns a client/state object
      poll(client)  → called every loop, returns a serial message or None
      on_command()  → handles a command received from the ESP32
    """

    PREFIX: str = ""  # serial prefix this app owns, e.g. "SPOTIFY:", "CPU:"

    def init(self):
        """One-time setup. Return a client/state object (passed to poll and on_command)."""
        return None

    def poll(self, client) -> "str | None":
        """Return a formatted serial message to send, or None to skip this cycle."""
        return None

    def poll_binary(self, client) -> list:
        """Return a list of raw binary messages (e.g. images) to send, or [] to skip."""
        return []

    def on_command(self, cmd: str, client) -> None:
        """Handle a command string received from the ESP32 (e.g. button press)."""
        pass
