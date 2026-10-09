"""CI status orb: reports the latest GitHub Actions run, via the `gh` CLI.

`gh` finds the repository from the working directory and is already authenticated, so
there is nothing to configure.
"""

import json
import os
import subprocess
import time
from datetime import UTC, datetime
from typing import NamedTuple

from .base import App

FETCH_INTERVAL = 5  # seconds between gh calls (independent of the host poll cadence)
TITLE_MAX = 60

# Map GitHub Actions status/conclusion onto the four orb states the firmware knows.
RUNNING = {"queued", "in_progress", "requested", "waiting", "pending"}
FAILED = {"failure", "timed_out", "startup_failure"}

RUN_FIELDS = (
    "databaseId,headSha,status,conclusion,workflowName,headBranch,number,"
    "displayTitle,createdAt,updatedAt"
)


class CiStatus(NamedTuple):
    state: str  # running | success | failure | unknown
    repo: str
    title: str
    wf_branch: str
    runinfo: str

    def message(self) -> str:
        return f"CI:{self.state}|{self.repo}|{self.title}|{self.wf_branch}|{self.runinfo}\n"


def ascii_only(s: str) -> str:
    """Reduce to displayable ASCII (the device fonts are ASCII-only) and strip the delimiter."""
    s = s.replace("|", " ").replace("\n", " ").replace("\r", " ")
    return s.encode("ascii", "ignore").decode("ascii").strip()


def format_delta(iso: str, running: bool) -> str:
    """Compact 'just now' / '2m' / '1h 3m' / '2d', with ' ago' suffix when settled."""
    if not iso:
        return ""
    try:
        ts = datetime.fromisoformat(iso.replace("Z", "+00:00"))
    except ValueError:
        return ""
    secs = max(0, int((datetime.now(UTC) - ts).total_seconds()))

    if secs < 45:
        text = "just now"
    elif secs < 3600:
        text = f"{secs // 60}m"
    elif secs < 86400:
        h, m = divmod(secs // 60, 60)
        text = f"{h}h {m}m" if m else f"{h}h"
    else:
        text = f"{secs // 86400}d"

    if running or text == "just now":
        return text
    return f"{text} ago"


def _gh(args: list[str], cwd: str | None):
    """Run `gh` and return its parsed JSON output, or None if it fails."""
    try:
        out = subprocess.run(["gh", *args], cwd=cwd, capture_output=True, text=True, timeout=10)
        if out.returncode != 0:
            return None
        return json.loads(out.stdout or "null")
    except (OSError, subprocess.SubprocessError, ValueError):
        return None


def _gh_text(args: list[str], cwd: str | None) -> str:
    try:
        out = subprocess.run(["gh", *args], cwd=cwd, capture_output=True, text=True, timeout=10)
    except (OSError, subprocess.SubprocessError):
        return ""
    return out.stdout.strip() if out.returncode == 0 else ""


def job_progress(cwd: str | None, run_id) -> str:
    """While a run is in progress, summarize its jobs as 'done/total currentjob'."""
    if not run_id:
        return ""
    data = _gh(["run", "view", str(run_id), "--json", "jobs"], cwd)
    jobs = (data or {}).get("jobs", [])
    if not jobs:
        return ""

    done = sum(1 for j in jobs if (j.get("status") or "").lower() == "completed")
    current = next(
        (j.get("name", "") for j in jobs if (j.get("status") or "").lower() == "in_progress"), ""
    )

    progress = f"{done}/{len(jobs)}"
    if current:
        room = 28 - len(progress)  # leave space for a leading "#123 " prefix
        if room > 1:
            progress += f" {ascii_only(current)[: room - 1]}"
    return progress


def check_runs_progress(cwd: str | None, repo: str, sha: str | None) -> str:
    """Fallback: tally check-runs for the commit as 'done/total checks'."""
    if not repo or not sha:
        return ""
    statuses = _gh(
        ["api", f"repos/{repo}/commits/{sha}/check-runs", "--jq", "[.check_runs[].status]"], cwd
    )
    if not statuses:
        return ""
    done = sum(1 for s in statuses if s == "completed")
    return f"{done}/{len(statuses)} checks"


def run_info(number, detail: str) -> str:
    """'#129  detail', leaving out whichever part is missing."""
    return "  ".join(part for part in (f"#{number}" if number else "", detail) if part)


class CiApp(App):
    def __init__(self, repo_dir: str | None = None):
        # Precedence: explicit argument > CI_REPO_DIR env > current directory (None).
        self._repo_dir = repo_dir or os.getenv("CI_REPO_DIR") or None
        self._repo_name: str | None = None  # owner/name; fixed for the life of the process
        self._last_fetch = 0.0
        self._last_msg: str | None = None

    def _repo(self) -> str:
        if self._repo_name is None:
            self._repo_name = _gh_text(
                ["repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"],
                self._repo_dir,
            )
        return self._repo_name

    def _status(self) -> CiStatus | None:
        """The latest run's status, or None if it could not be fetched."""
        runs = _gh(["run", "list", "--limit", "1", "--json", RUN_FIELDS], self._repo_dir)
        if runs is None:
            return None
        repo = self._repo()
        if not runs:
            return CiStatus("unknown", repo, "", "No runs", "")
        run = runs[0]

        status = (run.get("status") or "").lower()
        conclusion = (run.get("conclusion") or "").lower()
        if status in RUNNING:
            state = "running"
        elif status == "completed" and conclusion == "success":
            state = "success"
        elif status == "completed" and conclusion in FAILED:
            state = "failure"
        else:
            state = "unknown"

        title = ascii_only(run.get("displayTitle", ""))[:TITLE_MAX]
        wf_branch = ascii_only(f"{run.get('workflowName', 'CI')} / {run.get('headBranch', '')}")
        wf_branch = wf_branch.strip(" /")

        if state == "running":
            detail = job_progress(self._repo_dir, run.get("databaseId")) or check_runs_progress(
                self._repo_dir, repo, run.get("headSha")
            )
            detail = detail or format_delta(run.get("createdAt", ""), running=True)
        else:
            detail = format_delta(run.get("updatedAt", ""), running=False)

        return CiStatus(state, repo, title, wf_branch, run_info(run.get("number"), detail))

    def on_activate(self) -> None:
        # Fetch at once and re-send even if unchanged: the device forgets while on standby.
        self._last_fetch = 0.0
        self._last_msg = None

    def poll(self) -> list[str | bytes]:
        now = time.monotonic()
        if now - self._last_fetch < FETCH_INTERVAL:
            return []
        self._last_fetch = now

        status = self._status()
        if status is None:
            return []

        msg = status.message()
        if msg == self._last_msg:
            return []  # only push on change
        self._last_msg = msg
        print(f"CI: {status.state} {status.runinfo} - {status.title} ({status.wf_branch})")
        return [msg]

    def on_command(self, cmd: str) -> None:
        if cmd == "ci_refresh":
            print(">>> Command: ci_refresh")
            # Force the next poll to fetch immediately and re-send even if unchanged.
            self._last_fetch = 0.0
            self._last_msg = None
        elif cmd == "ci_open":
            print(">>> Command: ci_open")
            try:
                subprocess.run(["gh", "run", "view", "--web"], cwd=self._repo_dir, timeout=10)
            except (OSError, subprocess.SubprocessError) as e:
                print(f"CI open failed: {e}")
