import os
import time

import ci_monitor
from app import App

FETCH_INTERVAL = 5  # seconds between gh calls (independent of the host poll cadence)


class CiApp(App):
    PREFIX = "CI:"

    def __init__(self, repo_dir=None):
        # Precedence: explicit arg > CI_REPO_DIR env > current directory (None).
        self._repo_override = repo_dir

    def init(self):
        self._repo_dir = self._repo_override or os.getenv("CI_REPO_DIR") or None
        self._last_fetch = 0.0
        self._last_msg = None
        return None  # no client needed

    def poll(self, _) -> "str | None":
        now = time.monotonic()
        if now - self._last_fetch < FETCH_INTERVAL:
            return None
        self._last_fetch = now

        status = ci_monitor.get_status(self._repo_dir)
        if status is None:
            return None
        state, repo, title, wf_branch, runinfo = status

        msg = ci_monitor.format_msg(state, repo, title, wf_branch, runinfo)
        if msg == self._last_msg:
            return None  # only push on change
        self._last_msg = msg
        print(f"CI: {state} {runinfo} — {title} ({wf_branch})")
        return msg

    def on_command(self, cmd: str, _) -> None:
        if cmd == "ci_refresh":
            print(">>> Command: ci_refresh")
            # Force the next poll to fetch immediately and re-send even if unchanged.
            self._last_fetch = 0.0
            self._last_msg = None
        elif cmd == "ci_open":
            print(">>> Command: ci_open")
            ci_monitor.open_latest(self._repo_dir)
