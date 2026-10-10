import threading

from .apps import App


class Runner:
    """Runs the apps, but only the one the device has on screen; the others are on standby.

    The device reports its active app from the serial thread; everything else, including
    ``App.on_activate``, happens on the polling thread.
    """

    def __init__(self, apps: dict[str, App]):
        self._apps = apps
        self._lock = threading.Lock()
        self._requested: str | None = None
        self._active: str | None = None
        self._switched = threading.Event()

    @property
    def known(self) -> bool:
        """Whether the device has said which app is on screen."""
        return self._requested is not None

    def device_showing(self, name: str) -> None:
        """The device now shows app `name`. Wakes up wait()."""
        with self._lock:
            self._requested = name
        self._switched.set()

    def on_command(self, cmd: str) -> None:
        app = self._apps.get(self._requested or "")
        if app is not None:
            app.on_command(cmd)

    def wait(self, timeout: float) -> None:
        """Sleep until `timeout` passes or the device switches app."""
        self._switched.wait(timeout)
        self._switched.clear()

    def poll(self) -> list[str | bytes]:
        """The active app's messages. Hands over to a newly shown app first."""
        with self._lock:
            name = self._requested
        if name != self._active:
            self._active = name
            if name in self._apps:
                print(f"Showing the {name} app.")
                self._apps[name].on_activate()
            else:
                print(f"The device shows '{name}', which is not running here.")
        app = self._apps.get(self._active or "")
        return app.poll() if app is not None else []
