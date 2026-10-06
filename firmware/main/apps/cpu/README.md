# cpu

A radial gauge of the host computer's CPU usage: a 270° arc with the percentage in the middle.

No touch interaction.

## Host

Needs the `cpu` extra ([`psutil`](https://pypi.org/project/psutil/)). No other setup.

```bash
pip install -e ".[cpu]"
dot-host cpu
```

The host sends the usage once per poll cycle (about every second).

## Protocol

```
CPU:<percent>
```

`percent` is a number from 0 to 100 with one decimal, for example `CPU:37.5`. The device sends
nothing back.

## Files

- Firmware: [`cpu.c`](cpu.c) (parsing), [`cpu_ui.c`](cpu_ui.c) (the gauge)
- Host: [`host/dot_host/apps/cpu.py`](../../../../host/dot_host/apps/cpu.py)

This is the smallest app, so it is the best one to copy when writing a new one.
