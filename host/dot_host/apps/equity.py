"""EquityWatch portfolio: total return on the display, with amounts shown on a tap.

Signs in to a running EquityWatch the way the browser does and reads ``/api/portfolio``. The
URL and credentials come from the environment (``host/.env``); only percentages go to the
device until it asks for the amounts with a tap.
"""

import os
import time
from datetime import UTC, datetime
from decimal import ROUND_HALF_UP, Decimal, InvalidOperation
from typing import Any

import requests
from dotenv import load_dotenv

from .base import App

FETCH_INTERVAL = 60  # seconds; EquityWatch caches prices for 5 minutes anyway
REQUEST_TIMEOUT = 10
STALE_AFTER_S = 15 * 60
AED_PER_USD_FALLBACK = Decimal("3.6725")
TOP_COUNT = 3
DEBUG_NUDGE = Decimal("0.5")  # percentage points added to the total return while testing
SYMBOL_MAX = 11  # the device keeps 11 characters


def _dec(value: Any) -> Decimal:
    """A number as the API sends it: a string such as "12.5", an int, a float or null."""
    try:
        return Decimal(str(value))
    except (InvalidOperation, ValueError):
        return Decimal(0)


def percent(part: Decimal, whole: Decimal) -> Decimal:
    return part / whole * 100 if whole else Decimal(0)


def price_age_s(prices_as_of: str | None, now: datetime) -> int | None:
    if not prices_as_of:
        return None
    try:
        as_of = datetime.fromisoformat(prices_as_of.replace("Z", "+00:00"))
    except ValueError:
        return None
    if as_of.tzinfo is None:
        as_of = as_of.replace(tzinfo=UTC)
    return max(0, int((now - as_of).total_seconds()))


def summary_message(portfolio: dict, now: datetime, nudge: Decimal = Decimal(0)) -> str:
    """``EQ:S|<state>|<total %>|<unrealized %>|<positions>|<age s>`` from a /api/portfolio body.

    ``nudge`` shifts the total return; it is only used to test the update flash on the device.
    """
    totals = portfolio.get("totals", {})
    cost = _dec(totals.get("cost_basis"))
    total_pct = percent(_dec(totals.get("total_gain")), cost) + nudge
    unrealized_pct = _dec(totals.get("unrealized_gain_pct"))
    positions = portfolio.get("positions", [])
    age = price_age_s(portfolio.get("prices_as_of"), now)

    stale = (
        bool(portfolio.get("missing_prices"))
        or any(p.get("stale") for p in positions)
        or (age is not None and age > STALE_AFTER_S)
    )
    state = "stale" if stale else "ok"
    return (
        f"EQ:S|{state}|{total_pct:.2f}|{unrealized_pct:.2f}|{len(positions)}|"
        f"{-1 if age is None else age}\n"
    )


def top_message(portfolio: dict) -> str:
    """``EQ:T|<symbol>|<percent>...``: the best performing holdings, best first.

    Ranked by unrealized return; holdings without a price are left out. With nothing to show the
    line is a bare ``EQ:T``, which clears the list on the device.
    """
    ranked = []
    for position in portfolio.get("positions", []):
        pct = position.get("unrealized_gain_pct")
        if pct is None:
            continue
        symbol = str(position.get("symbol", "")).replace("|", " ").encode("ascii", "ignore")
        ranked.append((_dec(pct), symbol.decode()[:SYMBOL_MAX]))
    ranked.sort(key=lambda r: r[0], reverse=True)
    fields = "".join(f"|{symbol}|{pct:.2f}" for pct, symbol in ranked[:TOP_COUNT])
    return f"EQ:T{fields}\n"


def status_message(state: str) -> str:
    """A summary with no numbers, for ``offline`` and ``auth``: the device keeps the old ones."""
    return f"EQ:S|{state}|0|0|0|-1\n"


def amounts_message(portfolio: dict, currency: str = "AED") -> str:
    """``EQ:V|<value>|<gain>|<currency>``. The API's totals are USD; convert at its rate."""
    totals = portfolio.get("totals", {})
    rate = _dec(portfolio.get("rates", {}).get(currency)) or AED_PER_USD_FALLBACK
    value = (_dec(totals.get("market_value")) * rate).quantize(Decimal(1), ROUND_HALF_UP)
    gain = (_dec(totals.get("total_gain")) * rate).quantize(Decimal(1), ROUND_HALF_UP)
    return f"EQ:V|{value:,.0f}|{gain:+,.0f}|{currency}\n"


class EquityApp(App):
    def __init__(self) -> None:
        self._url = ""
        self._username = ""
        self._password = ""
        self._session = requests.Session()
        self._signed_in = False
        self._last_fetch = 0.0
        self._portfolio: dict | None = None
        self._reveal = False
        self._debug_every = 0.0  # seconds between fake changes; 0 is off
        self._last_debug = 0.0
        self._nudge = Decimal(0)

    def start(self) -> None:
        load_dotenv()  # EQUITYWATCH_URL / _USER / _PASSWORD, from host/.env
        self._url = os.getenv("EQUITYWATCH_URL", "http://localhost").rstrip("/")
        self._username = os.getenv("EQUITYWATCH_USER", "")
        self._password = os.getenv("EQUITYWATCH_PASSWORD", "")
        try:
            self._debug_every = float(os.getenv("EQUITYWATCH_DEBUG_FLASH", "0") or 0)
        except ValueError:
            self._debug_every = 0.0
        if self._debug_every > 0:
            print(
                f"EquityWatch: DEBUG, the return is nudged by {DEBUG_NUDGE} points every "
                f"{self._debug_every:g} s to test the update flash. Unset EQUITYWATCH_DEBUG_FLASH."
            )
        if not (self._username and self._password):
            print("EquityWatch: set EQUITYWATCH_USER and EQUITYWATCH_PASSWORD in host/.env")

    def _sign_in(self) -> bool:
        resp = self._session.post(
            f"{self._url}/api/auth/login",
            json={"username": self._username, "password": self._password},
            timeout=REQUEST_TIMEOUT,
        )
        self._signed_in = resp.status_code == 204
        return self._signed_in

    def _get_portfolio(self) -> requests.Response:
        return self._session.get(f"{self._url}/api/portfolio", timeout=REQUEST_TIMEOUT)

    def _fetch(self) -> str:
        """Refresh the portfolio. Returns "ok", "offline" or "auth"."""
        try:
            if not self._signed_in and not self._sign_in():
                return "auth"
            resp = self._get_portfolio()
            if resp.status_code == 401:  # the session expired: sign in once more
                if not self._sign_in():
                    return "auth"
                resp = self._get_portfolio()
            resp.raise_for_status()
            self._portfolio = resp.json()
        except (requests.RequestException, ValueError) as e:
            print(f"EquityWatch: {e}")
            self._signed_in = False
            return "offline"
        return "ok"

    def on_activate(self) -> None:
        self._last_fetch = 0.0

    def on_command(self, cmd: str) -> None:
        if cmd == "eq_reveal":
            print(">>> Command: eq_reveal")
            self._reveal = True

    def _debug_step(self, now: float) -> bool:
        """Alternate the nudge on and off so the device sees the return rise, then fall."""
        if (
            self._debug_every <= 0
            or self._portfolio is None
            or now - self._last_debug < self._debug_every
        ):
            return False
        self._last_debug = now
        self._nudge = DEBUG_NUDGE if self._nudge == 0 else Decimal(0)
        return True

    def poll(self) -> list[str | bytes]:
        messages: list[str | bytes] = []
        now = time.monotonic()
        send_summary = False
        if now - self._last_fetch >= FETCH_INTERVAL:
            self._last_fetch = now
            result = self._fetch()
            if result == "ok":
                send_summary = True
            else:
                messages.append(status_message(result))
        if self._debug_step(now):
            send_summary = True
        if send_summary and self._portfolio is not None:
            msg = summary_message(self._portfolio, datetime.now(UTC), self._nudge)
            print(f"EquityWatch: {msg.strip().removeprefix('EQ:S|')}")
            messages.append(msg)
            messages.append(top_message(self._portfolio))
        if self._reveal and self._portfolio is not None:
            self._reveal = False
            messages.append(amounts_message(self._portfolio))
        return messages
