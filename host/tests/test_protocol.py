"""The host half of the serial protocol: what each app puts on the wire."""

from datetime import UTC, datetime

import requests

from dot_host.apps import ci, cpu, equity, spotify


def test_cpu_message():
    assert cpu.format_msg(42.345) == "CPU:42.3\n"


def test_ci_message():
    status = ci.CiStatus("running", "me/repo", "fix bug", "CI / main", "#7  1/3 test")
    assert status.message() == "CI:running|me/repo|fix bug|CI / main|#7  1/3 test\n"


def test_ci_text_is_ascii_and_free_of_delimiters():
    assert ci.ascii_only("fix: café | naïve\nline") == "fix: caf   nave line"


def test_ci_run_info_skips_missing_parts():
    assert ci.run_info(129, "2m ago") == "#129  2m ago"
    assert ci.run_info(129, "") == "#129"
    assert ci.run_info(None, "just now") == "just now"
    assert ci.run_info(None, "") == ""


def test_ci_delta_is_compact():
    assert ci.format_delta("", running=False) == ""
    assert ci.format_delta("not a timestamp", running=False) == ""


def test_spotify_message_normalizes_text():
    msg = spotify.format_msg("Don’t | Stop", "A—B", True, 1000, 200000)
    assert msg == "SPOTIFY:Don't   Stop|A-B|1|1000|200000\n"


def test_spotify_image_header():
    blob = spotify.format_img_msg(b"\x01\x02", 180, 180, (255, 0, 16))
    assert blob == b"IMG:180x180:2:ff0010\n\x01\x02"


def test_spotify_picks_smallest_art_that_fits():
    images = [
        {"url": "big", "width": 640},
        {"url": "mid", "width": 300},
        {"url": "tiny", "width": 64},
    ]
    assert spotify.pick_art_url(images) == "mid"
    assert spotify.pick_art_url([{"url": "tiny", "width": 64}]) == "tiny"
    assert spotify.pick_art_url([]) is None


PORTFOLIO = {
    "positions": [{"stale": False}, {"stale": False}, {"stale": False}],
    "totals": {
        "market_value": "1000",
        "cost_basis": "800",
        "unrealized_gain_pct": "25",
        "total_gain": "200",
    },
    "prices_as_of": "2026-10-10T10:00:00Z",
    "missing_prices": [],
    "rates": {"USD": "1", "AED": "3.6725"},
}
NOW = datetime(2026, 10, 10, 10, 2, tzinfo=UTC)


def test_equity_summary_message():
    assert equity.summary_message(PORTFOLIO, NOW) == "EQ:S|ok|25.00|25.00|3|120\n"


def test_equity_total_return_includes_realized_gains():
    portfolio = {**PORTFOLIO, "totals": {**PORTFOLIO["totals"], "total_gain": "300"}}
    assert equity.summary_message(portfolio, NOW).startswith("EQ:S|ok|37.50|25.00|")


def test_equity_summary_with_nothing_held():
    empty = {"positions": [], "totals": {"cost_basis": "0", "total_gain": "0"}}
    assert equity.summary_message(empty, NOW) == "EQ:S|ok|0.00|0.00|0|-1\n"


def test_equity_stale_prices():
    missing = {**PORTFOLIO, "missing_prices": ["AAPL"]}
    assert equity.summary_message(missing, NOW).startswith("EQ:S|stale|")
    old = {**PORTFOLIO, "prices_as_of": "2026-10-10T09:00:00Z"}
    assert equity.summary_message(old, NOW).startswith("EQ:S|stale|")
    flagged = {**PORTFOLIO, "positions": [{"stale": True}]}
    assert equity.summary_message(flagged, NOW).startswith("EQ:S|stale|")


def test_equity_status_message_has_no_numbers():
    assert equity.status_message("offline") == "EQ:S|offline|0|0|0|-1\n"


def test_equity_amounts_are_converted_to_aed():
    assert equity.amounts_message(PORTFOLIO) == "EQ:V|3,673|+735|AED\n"
    loss = {**PORTFOLIO, "totals": {**PORTFOLIO["totals"], "total_gain": "-200"}}
    assert equity.amounts_message(loss) == "EQ:V|3,673|-735|AED\n"


class FakeResponse:
    def __init__(self, status_code, body=None):
        self.status_code = status_code
        self._body = body

    def json(self):
        return self._body

    def raise_for_status(self):
        if self.status_code >= 400:
            raise requests.HTTPError(str(self.status_code))


class FakeSession:
    def __init__(self, login_status=204, portfolio_statuses=(200,)):
        self.login_status = login_status
        self.portfolio_statuses = list(portfolio_statuses)
        self.logins = 0

    def post(self, url, json, timeout):
        self.logins += 1
        return FakeResponse(self.login_status)

    def get(self, url, timeout):
        status = self.portfolio_statuses.pop(0) if self.portfolio_statuses else 200
        return FakeResponse(status, PORTFOLIO)


def make_equity_app(session):
    app = equity.EquityApp()
    app._session = session
    app._clock = lambda: NOW
    return app


def test_equity_poll_sends_the_summary_and_signs_in_once():
    session = FakeSession()
    app = make_equity_app(session)
    messages = app.poll()
    assert len(messages) == 2
    assert messages[0].startswith("EQ:S|") and messages[1].startswith("EQ:T")
    assert session.logins == 1


def test_equity_signs_in_again_when_the_session_expires():
    session = FakeSession(portfolio_statuses=(401, 200))
    app = make_equity_app(session)
    assert app.poll()[0].startswith("EQ:S|ok|")
    assert session.logins == 2


def test_equity_reports_refused_credentials():
    app = make_equity_app(FakeSession(login_status=401))
    assert app.poll() == ["EQ:S|auth|0|0|0|-1\n"]


def test_equity_reports_an_unreachable_server():
    class Down(FakeSession):
        def post(self, url, json, timeout):
            raise requests.ConnectionError("refused")

    assert make_equity_app(Down()).poll() == ["EQ:S|offline|0|0|0|-1\n"]


def test_equity_amounts_only_follow_a_tap():
    app = make_equity_app(FakeSession())
    assert not any(m.startswith("EQ:V") for m in app.poll())  # no amounts yet
    app.on_command("eq_reveal")
    assert app.poll() == ["EQ:V|3,673|+735|AED\n"]
    assert app.poll() == []


def positions(*pairs):
    return {"positions": [{"symbol": s, "unrealized_gain_pct": p} for s, p in pairs]}


def test_equity_top_three_best_first():
    portfolio = positions(
        ("AAA", "5"), ("BBB", "30.126"), ("CCC", "-4"), ("DDD", "12"), ("EEE", "1")
    )
    assert equity.top_message(portfolio) == "EQ:T|BBB|30.13|DDD|12.00|AAA|5.00\n"


def test_equity_top_includes_losers_when_few_holdings():
    portfolio = positions(("AAA", "-2.5"), ("BBB", "-9"))
    assert equity.top_message(portfolio) == "EQ:T|AAA|-2.50|BBB|-9.00\n"


def test_equity_top_skips_unpriced_holdings():
    portfolio = positions(("AAA", None), ("BBB", "3"))
    assert equity.top_message(portfolio) == "EQ:T|BBB|3.00\n"


def test_equity_top_with_nothing_clears_the_list():
    assert equity.top_message({"positions": []}) == "EQ:T\n"
    assert equity.top_message(positions(("AAA", None))) == "EQ:T\n"


def test_equity_top_symbols_are_safe_for_the_protocol():
    portfolio = positions(("A|B", "1"), ("GOLD-22K", "2"), ("VERYLONGSYMBOLNAME", "0"))
    msg = equity.top_message(portfolio)
    assert msg == "EQ:T|GOLD-22K|2.00|A B|1.00|VERYLONGSYM|0.00\n"
