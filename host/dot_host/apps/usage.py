"""Claude Code token usage: today, the last 5 hours and the last 7 days.

Reads the transcripts Claude Code keeps under ``~/.claude/projects`` (override with
``CLAUDE_CONFIG_DIR``). Every assistant message there carries a ``usage`` block; streamed
chunks repeat the same message id, so each id is counted once. Cache reads are left out of the
totals: they are re-reads of the same context and would swamp the real work.

The percentages and reset times are your real plan utilization, the way Clawdmeter reads them:
the app takes Claude Code's own login token from ``<config dir>/.credentials.json`` (it never
refreshes it; Claude Code does) and sends one 1-token request to ``api.anthropic.com`` every
minute, which answers with ``anthropic-ratelimit-unified-5h/7d-*`` headers. The token goes
nowhere else and is never printed. That request counts against your plan, though only barely.

If that is unavailable (no token on this machine, expired, offline) the percentages fall back to
budgets you choose, ``CLAUDE_5H_LIMIT`` and ``CLAUDE_7D_LIMIT`` in tokens, from the environment or
``host/.env``; with neither, the device shows token counts alone.
"""

import json
import os
import time
import urllib.error
import urllib.request
from collections.abc import Iterator
from datetime import UTC, datetime, timedelta
from pathlib import Path

from .base import App

SCAN_INTERVAL = 30  # seconds between scans of the transcripts
WINDOW_5H = timedelta(hours=5)
WINDOW_7D = timedelta(days=7)
TOKEN_FIELDS = ("input_tokens", "output_tokens", "cache_creation_input_tokens")

LIMITS_INTERVAL = 60  # seconds between requests for the plan utilization
REQUEST_TIMEOUT = 20
API_URL = "https://api.anthropic.com/v1/messages"
API_BODY = {
    "model": "claude-haiku-4-5-20251001",
    "max_tokens": 1,
    "messages": [{"role": "user", "content": "hi"}],
}
RATELIMIT = "anthropic-ratelimit-unified-"


def config_dir() -> Path:
    base = os.getenv("CLAUDE_CONFIG_DIR")
    return Path(base) if base else Path.home() / ".claude"


def read_token(directory: Path) -> str | None:
    """Claude Code's OAuth access token from ``.credentials.json``, if there is one here."""
    try:
        data = json.loads((directory / ".credentials.json").read_text())
        token = (data.get("claudeAiOauth") or data).get("accessToken")
    except (OSError, ValueError, AttributeError):
        return None
    return token if isinstance(token, str) and token else None


class LimitsUnavailable(Exception):
    """The plan utilization could not be read; the caller falls back to the local counts."""


def parse_limits(headers, now: float) -> dict[str, tuple[int, int]]:
    """``{"5h": (percent, minutes to reset), "7d": ...}`` from the rate-limit response headers.

    Raises LimitsUnavailable when the account has no 5-hour/7-day windows (e.g. Enterprise).
    """
    limits: dict[str, tuple[int, int]] = {}
    for window in ("5h", "7d"):
        try:
            used = float(headers[f"{RATELIMIT}{window}-utilization"])
            reset = float(headers[f"{RATELIMIT}{window}-reset"])
        except (KeyError, TypeError, ValueError):
            raise LimitsUnavailable(f"no {window} rate-limit headers") from None
        limits[window] = (round(used * 100), max(0, round((reset - now) / 60)))
    return limits


def fetch_limits(token: str) -> dict[str, tuple[int, int]]:
    """Ask the API for the plan utilization with a minimal request."""
    request = urllib.request.Request(
        API_URL,
        data=json.dumps(API_BODY).encode(),
        headers={
            "Authorization": f"Bearer {token}",
            "anthropic-version": "2023-06-01",
            "anthropic-beta": "oauth-2025-04-20",
            "Content-Type": "application/json",
            "User-Agent": "claude-code/2.1.5",
        },
    )
    try:
        with urllib.request.urlopen(request, timeout=REQUEST_TIMEOUT) as response:
            return parse_limits(response.headers, time.time())
    except urllib.error.HTTPError as e:
        if e.code in (401, 403):
            raise LimitsUnavailable(
                "the login token is expired; use Claude Code to renew it"
            ) from e
        raise LimitsUnavailable(f"HTTP {e.code}") from e
    except (urllib.error.URLError, OSError) as e:
        raise LimitsUnavailable(str(e)) from e


def parse_time(stamp: str) -> datetime | None:
    try:
        parsed = datetime.fromisoformat(stamp.replace("Z", "+00:00"))
    except (ValueError, AttributeError):
        return None
    return parsed if parsed.tzinfo else parsed.replace(tzinfo=UTC)


def read_entries(path: Path) -> Iterator[tuple[str, datetime, int]]:
    """``(message id, time, tokens)`` for each assistant message in one transcript."""
    try:
        with path.open(encoding="utf-8", errors="replace") as f:
            for line in f:
                if '"usage"' not in line:  # cheap filter before parsing
                    continue
                try:
                    entry = json.loads(line)
                    message = entry["message"]
                    usage = message["usage"]
                    msg_id = message.get("id") or entry.get("requestId")
                except (ValueError, KeyError, TypeError):
                    continue
                when = parse_time(entry.get("timestamp", ""))
                if entry.get("type") != "assistant" or not msg_id or when is None:
                    continue
                tokens = sum(int(usage.get(field) or 0) for field in TOKEN_FIELDS)
                yield str(msg_id), when, tokens
    except OSError:
        return


def totals(entries: dict[str, tuple[datetime, int]], now: datetime) -> tuple[int, int, int, int]:
    """``(today, last 5h, last 7d, messages today)``; "today" is the local calendar day."""
    midnight = now.astimezone().replace(hour=0, minute=0, second=0, microsecond=0)
    today = five_hours = week = messages = 0
    for when, tokens in entries.values():
        if now - when <= WINDOW_7D:
            week += tokens
        if now - when <= WINDOW_5H:
            five_hours += tokens
        if when >= midnight:
            today += tokens
            messages += 1
    return today, five_hours, week, messages


def format_tokens(count: int) -> str:
    if count >= 1_000_000:
        return f"{count / 1_000_000:.1f}M"
    if count >= 1_000:
        return f"{count / 1_000:.1f}k"
    return str(count)


def limit_from_env(name: str) -> int:
    """A token budget from the environment; 0 (no budget) if unset or not a positive number."""
    try:
        return max(0, int(os.getenv(name, "0").replace("_", "").replace(",", "")))
    except ValueError:
        return 0


def percent_field(tokens: int, limit: int) -> str:
    """Whole percent of the budget used (may exceed 100), or ``-1`` without a budget."""
    return str(round(tokens / limit * 100)) if limit > 0 else "-1"


def summary_message(
    today: int,
    five_hours: int,
    week: int,
    messages: int,
    pct_5h: str = "-1",
    pct_7d: str = "-1",
    reset_5h: int = -1,
    reset_7d: int = -1,
) -> str:
    """``US:S|<today>|<5h>|<7d>|<messages today>|<5h %>|<7d %>|<5h reset min>|<7d reset min>``.

    The token counts are already abbreviated. A percentage or reset time of ``-1`` is unknown.
    """
    return (
        f"US:S|{format_tokens(today)}|{format_tokens(five_hours)}|{format_tokens(week)}|"
        f"{messages}|{pct_5h}|{pct_7d}|{reset_5h}|{reset_7d}\n"
    )


class UsageApp(App):
    def __init__(self, root: Path | None = None, config: Path | None = None) -> None:
        self._config_dir = config or config_dir()
        self._root = root or self._config_dir / "projects"
        self._clock = lambda: datetime.now(UTC)
        self._files: dict[Path, tuple[float, int, dict[str, tuple[datetime, int]]]] = {}
        self._last_scan: float | None = None
        self._limit_5h = 0
        self._limit_7d = 0
        self._limits: dict[str, tuple[int, int]] | None = None
        self._last_limits: float | None = None

    def start(self) -> None:
        try:
            from dotenv import load_dotenv

            load_dotenv()  # CLAUDE_5H_LIMIT / CLAUDE_7D_LIMIT, from host/.env, if python-dotenv is there
        except ImportError:
            pass
        self._limit_5h = limit_from_env("CLAUDE_5H_LIMIT")
        self._limit_7d = limit_from_env("CLAUDE_7D_LIMIT")
        if read_token(self._config_dir) is None:
            print(
                f"Usage: no Claude Code login in {self._config_dir}; percentages need "
                "CLAUDE_5H_LIMIT / CLAUDE_7D_LIMIT (tokens)"
            )
        if not self._root.is_dir():
            print(f"Usage: no Claude Code transcripts at {self._root}")

    def on_activate(self) -> None:
        self._last_scan = None

    def _scan(self) -> dict[str, tuple[datetime, int]]:
        """Merge every recent transcript, re-reading only the files that changed."""
        cutoff = time.time() - WINDOW_7D.total_seconds()
        seen: dict[Path, tuple[float, int, dict[str, tuple[datetime, int]]]] = {}
        merged: dict[str, tuple[datetime, int]] = {}
        for path in self._root.glob("**/*.jsonl"):
            try:
                stat = path.stat()
            except OSError:
                continue
            if stat.st_mtime < cutoff:
                continue
            cached = self._files.get(path)
            if cached and cached[0] == stat.st_mtime and cached[1] == stat.st_size:
                entries = cached[2]
            else:
                entries = {i: (when, tokens) for i, when, tokens in read_entries(path)}
            seen[path] = (stat.st_mtime, stat.st_size, entries)
            for msg_id, value in entries.items():
                # a message can appear in several files (resumed sessions): keep the largest
                if msg_id not in merged or value[1] > merged[msg_id][1]:
                    merged[msg_id] = value
        self._files = seen
        return merged

    def _refresh_limits(self, now: float) -> None:
        """Ask for the plan utilization at most once a minute, keeping the last answer."""
        if self._last_limits is not None and now - self._last_limits < LIMITS_INTERVAL:
            return
        self._last_limits = now
        token = read_token(self._config_dir)
        if token is None:
            self._limits = None
            return
        try:
            self._limits = fetch_limits(token)
        except LimitsUnavailable as e:
            print(f"Usage: plan utilization unavailable: {e}")
            self._limits = None

    def poll(self) -> list[str | bytes]:
        now = time.monotonic()
        if self._last_scan is not None and now - self._last_scan < SCAN_INTERVAL:
            return []
        self._last_scan = now
        self._refresh_limits(now)
        today, five_hours, week, messages = totals(self._scan(), self._clock())
        if self._limits:
            (pct_5h, reset_5h), (pct_7d, reset_7d) = self._limits["5h"], self._limits["7d"]
            percents = (str(pct_5h), str(pct_7d), reset_5h, reset_7d)
        else:
            percents = (
                percent_field(five_hours, self._limit_5h),
                percent_field(week, self._limit_7d),
                -1,
                -1,
            )
        msg = summary_message(today, five_hours, week, messages, *percents)
        print(f"Usage: {msg.strip().removeprefix('US:S|')}")
        return [msg]
