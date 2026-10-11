import json
from datetime import UTC, datetime, timedelta

from dot_host.apps.usage import (
    LimitsUnavailable,
    UsageApp,
    format_tokens,
    limit_from_env,
    parse_limits,
    percent_field,
    read_token,
    summary_message,
    totals,
)

NOW = datetime(2026, 10, 10, 12, 0, tzinfo=UTC)


def line(msg_id: str, when: datetime, **usage) -> str:
    return json.dumps(
        {
            "type": "assistant",
            "timestamp": when.isoformat().replace("+00:00", "Z"),
            "message": {"id": msg_id, "usage": usage},
        }
    )


def make_app(tmp_path, lines: list[str]) -> UsageApp:
    (tmp_path / "proj").mkdir()
    (tmp_path / "proj" / "s.jsonl").write_text("\n".join(lines) + "\n")
    app = UsageApp(root=tmp_path, config=tmp_path)
    app._clock = lambda: datetime.now(UTC)
    return app


def test_format_tokens():
    assert [format_tokens(n) for n in (999, 1_500, 2_300_000)] == ["999", "1.5k", "2.3M"]


def test_summary_message():
    assert summary_message(1500, 20, 3_000_000, 4) == "US:S|1.5k|20|3.0M|4|-1|-1|-1|-1\n"
    message = summary_message(1500, 500, 3_000_000, 4, "25", "150")
    assert message == "US:S|1.5k|500|3.0M|4|25|150|-1|-1\n"


def test_percent_field():
    assert percent_field(500, 2000) == "25"
    assert percent_field(500, 0) == "-1"


def test_limit_from_env(monkeypatch):
    monkeypatch.delenv("X_LIMIT", raising=False)
    assert limit_from_env("X_LIMIT") == 0
    monkeypatch.setenv("X_LIMIT", "2_000,000")
    assert limit_from_env("X_LIMIT") == 2_000_000
    monkeypatch.setenv("X_LIMIT", "lots")
    assert limit_from_env("X_LIMIT") == 0
    monkeypatch.setenv("X_LIMIT", "-5")
    assert limit_from_env("X_LIMIT") == 0


def test_windows_split_by_age():
    entries = {
        "a": (NOW - timedelta(hours=1), 10),
        "b": (NOW - timedelta(hours=6), 100),
        "c": (NOW - timedelta(days=8), 1000),
    }
    _, five_hours, week, _ = totals(entries, NOW)
    assert (five_hours, week) == (10, 110)


def test_streamed_chunks_count_once_and_cache_reads_are_ignored(tmp_path):
    when = datetime.now(UTC)
    chunk = line(
        "m1",
        when,
        input_tokens=2,
        output_tokens=8,
        cache_creation_input_tokens=10,
        cache_read_input_tokens=99_999,
    )
    app = make_app(tmp_path, [chunk, chunk, "not json", '{"usage": 1}'])
    (message,) = app.poll()
    assert message == "US:S|20|20|20|1|-1|-1|-1|-1\n"


def test_scans_are_throttled_until_the_app_is_reactivated(tmp_path):
    app = make_app(tmp_path, [line("m1", datetime.now(UTC), output_tokens=5)])
    assert len(app.poll()) == 1
    assert app.poll() == []
    app.on_activate()
    assert len(app.poll()) == 1


def test_missing_directory_sends_zeros(tmp_path):
    (message,) = UsageApp(root=tmp_path / "nope", config=tmp_path).poll()
    assert message == "US:S|0|0|0|0|-1|-1|-1|-1\n"


HEADERS = {
    "anthropic-ratelimit-unified-5h-utilization": "0.28",
    "anthropic-ratelimit-unified-5h-reset": "1000",
    "anthropic-ratelimit-unified-7d-utilization": "0.094",
    "anthropic-ratelimit-unified-7d-reset": "37000",
}


def test_parse_limits_reads_utilization_and_minutes_to_reset():
    assert parse_limits(HEADERS, now=700.0) == {"5h": (28, 5), "7d": (9, 605)}


def test_parse_limits_without_the_headers_is_unavailable():
    try:
        parse_limits({}, now=0.0)
    except LimitsUnavailable:
        return
    raise AssertionError("expected LimitsUnavailable")


def test_read_token(tmp_path):
    assert read_token(tmp_path) is None
    (tmp_path / ".credentials.json").write_text('{"claudeAiOauth": {"accessToken": "t0k"}}')
    assert read_token(tmp_path) == "t0k"
    (tmp_path / ".credentials.json").write_text("not json")
    assert read_token(tmp_path) is None


def test_poll_uses_the_plan_utilization_when_available(tmp_path, monkeypatch):
    (tmp_path / ".credentials.json").write_text('{"claudeAiOauth": {"accessToken": "t0k"}}')
    app = make_app(tmp_path, [line("m1", datetime.now(UTC), output_tokens=5)])
    seen = []
    monkeypatch.setattr(
        "dot_host.apps.usage.fetch_limits",
        lambda token: seen.append(token) or {"5h": (28, 295), "7d": (9, 9000)},
    )
    (message,) = app.poll()
    assert message.endswith("|28|9|295|9000\n")
    assert seen == ["t0k"]


def test_poll_falls_back_to_the_budgets_when_the_token_is_rejected(tmp_path, monkeypatch):
    (tmp_path / ".credentials.json").write_text('{"claudeAiOauth": {"accessToken": "t0k"}}')
    app = make_app(tmp_path, [line("m1", datetime.now(UTC), output_tokens=50)])
    app._limit_5h = 100

    def rejected(token):
        raise LimitsUnavailable("expired")

    monkeypatch.setattr("dot_host.apps.usage.fetch_limits", rejected)
    (message,) = app.poll()
    assert message.endswith("|50|-1|-1|-1\n")


def test_the_plan_utilization_is_requested_once_a_minute(tmp_path, monkeypatch):
    (tmp_path / ".credentials.json").write_text('{"claudeAiOauth": {"accessToken": "t0k"}}')
    app = make_app(tmp_path, [line("m1", datetime.now(UTC), output_tokens=5)])
    calls = []
    monkeypatch.setattr(
        "dot_host.apps.usage.fetch_limits",
        lambda token: calls.append(1) or {"5h": (1, 1), "7d": (1, 1)},
    )
    app.poll()
    app.on_activate()  # a rescan, but not a new request
    app.poll()
    assert len(calls) == 1
