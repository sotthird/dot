# equity

Your EquityWatch portfolio as one number: the total return, large, with an
arrow and green or red. Below it are the unrealized return, the number of positions and how old the
prices are.

**Idle:** after 20 seconds without a touch the title fades away, the figures glide slightly toward
the centre and the lines under the percentage change to a larger size; everything eases back on the
next touch. It only runs while this app is on screen.

**Touch:** tap the screen to show the portfolio value and total gain in AED for 5 seconds. The
amounts are never sent to the device until you tap: the host only sends percentages.

| Status line | Meaning |
|---|---|
| Prices 2 min ago | normal |
| amber, Prices stale | prices older than 15 minutes, some missing, or marked stale by EquityWatch |
| EquityWatch offline | the host cannot reach it; the last numbers stay on screen |
| EquityWatch sign-in failed | wrong username or password in the host's environment |

## Host

Needs the `equity` extra ([`requests`](https://pypi.org/project/requests/) and
[`python-dotenv`](https://pypi.org/project/python-dotenv/)) and a running EquityWatch.

```bash
pip install -e ".[equity]"
dot-host equity
```

The host signs in the way the browser does and reads `/api/portfolio` once a minute. Put these in
`host/.env` (git-ignored), or export them:

```
EQUITYWATCH_URL=http://localhost
EQUITYWATCH_USER=your-username
EQUITYWATCH_PASSWORD=your-password
```

`EQUITYWATCH_URL` is where the EquityWatch web page is served (its `/api` goes through the same
address); it defaults to `http://localhost`. If EquityWatch serves plain HTTP, its
`COOKIE_SECURE` must be `false`, or the login cookie is dropped.

The password stays on the host. Amounts are the API's USD totals converted to AED at the rate the
API returns (the fixed 3.6725 peg).

## Protocol

```
EQ:S|<state>|<total %>|<unrealized %>|<positions>|<age seconds>
EQ:V|<value>|<gain>|<currency>
```

`S` is the summary, sent every poll. `state` is `ok`, `stale`, `offline` or `auth`; for the last two
the numbers are zero and the device keeps its old ones. `total %` is total gain (realized and
unrealized) over cost; `age` is the age of the oldest price, or `-1` if unknown. `V` carries the
amounts as text (`EQ:V|3,673|+735|AED`) and is sent only after the device sends
`CMD:eq_reveal`, within about a second.

## Files

- Firmware: [`equity.c`](equity.c) (parsing), [`equity_ui.c`](equity_ui.c) (the screen)
- Host: [`host/dot_host/apps/equity.py`](../../../../host/dot_host/apps/equity.py)
