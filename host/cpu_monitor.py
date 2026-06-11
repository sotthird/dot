import psutil


def get_cpu(interval=1.0) -> float:
    return psutil.cpu_percent(interval=interval)


def format_msg(cpu: float) -> str:
    return f"CPU:{cpu:.1f}\n"
