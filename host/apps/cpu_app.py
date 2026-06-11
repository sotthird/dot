import cpu_monitor
from app import App


class CpuApp(App):
    PREFIX = "CPU:"

    def init(self):
        return None  # no client needed

    def poll(self, _) -> "str | None":
        # interval=None returns immediately using the last measured value
        cpu = cpu_monitor.get_cpu(interval=None)
        print(f"CPU: {cpu:.1f}%")
        return cpu_monitor.format_msg(cpu)
