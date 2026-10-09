class App:
    """The host half of a device app: it produces the messages the firmware app renders.

    Only the app on the device's screen is polled; the others are on standby.

    Mirrors the firmware's ``app_t``. Messages are protocol lines (``str``) or raw
    binary payloads such as images (``bytes``).
    """

    def start(self) -> None:
        """One-time setup, before the first poll."""

    def on_activate(self) -> None:
        """The app came on screen after standby (or the device just connected). Apps that only
        send on change should send their current state again on the next poll."""

    def poll(self) -> list[str | bytes]:
        """Called every cycle; return the messages to send now, if any."""
        return []

    def on_command(self, cmd: str) -> None:
        """Handle a command from the device, e.g. a button press."""
