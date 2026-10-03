import psutil

from .base import App


def format_msg(cpu: float) -> str:
    return f"CPU:{cpu:.1f}\n"


class CpuApp(App):
    def poll(self) -> list[str | bytes]:
        # interval=None returns immediately with the usage since the previous call
        cpu = float(psutil.cpu_percent(interval=None))
        print(f"CPU: {cpu:.1f}%")
        return [format_msg(cpu)]
