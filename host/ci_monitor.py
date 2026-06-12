import json
import subprocess
from datetime import UTC, datetime

# Map GitHub Actions status/conclusion onto the four orb states the firmware knows.
_RUNNING = {"queued", "in_progress", "requested", "waiting", "pending"}
_FAILED = {"failure", "timed_out", "startup_failure"}

_TITLE_MAX = 60

# Cached once — the repo (owner/name) the host is watching doesn't change at runtime.
_repo_cache = None


def _ascii(s: str) -> str:
    """Reduce to displayable ASCII (the device fonts are ASCII-only) and strip the delimiter."""
    s = s.replace("|", " ").replace("\n", " ").replace("\r", " ")
    return s.encode("ascii", "ignore").decode("ascii").strip()


def _repo(repo_dir) -> str:
    global _repo_cache
    if _repo_cache is not None:
        return _repo_cache
    try:
        out = subprocess.run(
            ["gh", "repo", "view", "--json", "nameWithOwner", "-q", ".nameWithOwner"],
            cwd=repo_dir,
            capture_output=True,
            text=True,
            timeout=10,
        )
        _repo_cache = out.stdout.strip() if out.returncode == 0 else ""
    except Exception:
        _repo_cache = ""
    return _repo_cache


def _fmt_delta(iso: str, running: bool) -> str:
    """Compact 'just now' / '2m' / '1h 3m' / '2d', with ' ago' suffix when settled."""
    if not iso:
        return ""
    try:
        ts = datetime.fromisoformat(iso.replace("Z", "+00:00"))
    except ValueError:
        return ""
    secs = int((datetime.now(UTC) - ts).total_seconds())
    if secs < 0:
        secs = 0

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


def get_status(repo_dir=None) -> "tuple[str, str, str, str, str] | None":
    """Return (state, repo, title, wf_branch, runinfo) for the latest run, or None on error.

    state is one of: running | success | failure | unknown. Uses the `gh` CLI, which
    auto-detects the repo from repo_dir (or the current directory) and is already authenticated.
    """
    try:
        out = subprocess.run(
            [
                "gh",
                "run",
                "list",
                "--limit",
                "1",
                "--json",
                "status,conclusion,workflowName,headBranch,number,displayTitle,createdAt,updatedAt",
            ],
            cwd=repo_dir,
            capture_output=True,
            text=True,
            timeout=10,
        )
        if out.returncode != 0:
            return None
        runs = json.loads(out.stdout or "[]")
        repo = _repo(repo_dir)
        if not runs:
            return "unknown", repo, "", "No runs", ""
        run = runs[0]

        status = (run.get("status") or "").lower()
        conclusion = (run.get("conclusion") or "").lower()
        if status in _RUNNING:
            state = "running"
        elif status == "completed" and conclusion == "success":
            state = "success"
        elif status == "completed" and conclusion in _FAILED:
            state = "failure"
        else:
            state = "unknown"

        title = _ascii(run.get("displayTitle", ""))[:_TITLE_MAX]
        wf_branch = _ascii(f"{run.get('workflowName', 'CI')} / {run.get('headBranch', '')}").strip(
            " /"
        )

        when = run.get("createdAt", "") if state == "running" else run.get("updatedAt", "")
        delta = _fmt_delta(when, running=(state == "running"))
        number = run.get("number")
        if number and delta:
            runinfo = f"#{number}  {delta}"
        elif number:
            runinfo = f"#{number}"
        else:
            runinfo = delta

        return state, repo, title, wf_branch, runinfo
    except Exception as e:
        print(f"CI status fetch failed: {e}")
        return None


def format_msg(state: str, repo: str, title: str, wf_branch: str, runinfo: str) -> str:
    return f"CI:{state}|{repo}|{title}|{wf_branch}|{runinfo}\n"


def open_latest(repo_dir=None) -> None:
    """Open the latest run in the browser (fired by the device's tap)."""
    try:
        subprocess.run(["gh", "run", "view", "--web"], cwd=repo_dir, timeout=10)
    except Exception as e:
        print(f"CI open failed: {e}")
