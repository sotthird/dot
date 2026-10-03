"""Host apps. Each is the Python half of the firmware app of the same name."""

from importlib import import_module

from .base import App

_APPS = {"ci": "CiApp", "cpu": "CpuApp", "spotify": "SpotifyApp"}
APP_NAMES = tuple(_APPS)

__all__ = ["APP_NAMES", "App", "create"]


def create(name: str, **options) -> App:
    """Instantiate an app by name. Modules are imported on demand so that an app's
    dependencies (``pip install 'dot-host[spotify]'``) are only needed when it runs."""
    try:
        module = import_module(f"{__name__}.{name}")
    except ImportError as e:
        raise SystemExit(
            f"The {name} app needs extra packages: pip install 'dot-host[{name}]'\n{e}"
        ) from e
    return getattr(module, _APPS[name])(**options)
