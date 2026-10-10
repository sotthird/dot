# equity

Your EquityWatch portfolio as one number: the total return, large, with an
arrow and green or red. Below it are the unrealized return, the number of positions and how old the
prices are.

**Idle:** after 20 seconds without a touch the title and the status row at the bottom fade away,
the figures glide up and the lines under the percentage change to a larger size. Your three best
performing holdings (by unrealized return, symbol and percentage) then fade in below them.
Everything eases back on the next touch. It only runs while this app is on screen.

**Update flash:** when new prices change the total return (at the precision shown), a border
flashes round the screen: green if it went up, red if it went down. It fades in quickly, holds for
under a second and fades away. Nothing flashes for an unchanged figure, for the first reading after
you open the app, or when only the status line changes.

**Touch:** tap the screen to show the portfolio value and total gain in AED for 5 seconds. They
replace the details under the percentage (and the status line, which goes blank), so nothing is
added to the screen; the details fade back afterwards. The amounts are never sent to the device
until you tap: the host only sends percentages.

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

### Testing the update flash

Set `EQUITYWATCH_DEBUG_FLASH=8` in `host/.env` (or the environment) and restart `dot-host`. Every 8
seconds the host then nudges the total return it sends by 0.5 points, up and then back down, so the
border flashes green, red, green, red... without waiting for prices to move. The log says when it is
on. The real figures are unchanged; remove the setting when you are done.

## Protocol

```
EQ:S|<state>|<total %>|<unrealized %>|<positions>|<age seconds>
EQ:V|<value>|<gain>|<currency>
EQ:T|<symbol>|<percent>|<symbol>|<percent>|<symbol>|<percent>
```

`S` is the summary, sent every poll. `state` is `ok`, `stale`, `offline` or `auth`; for the last two
the numbers are zero and the device keeps its old ones. `total %` is total gain (realized and
unrealized) over cost; `age` is the age of the oldest price, or `-1` if unknown. `T` lists up to three holdings by unrealized return, best first, in the same cycle as `S`; a bare
`EQ:T` clears the list. `V` carries the
amounts as text (`EQ:V|3,673|+735|AED`) and is sent only after the device sends
`CMD:eq_reveal`, within about a second.

## Files

- Firmware: [`equity.c`](equity.c) (parsing), [`equity_ui.c`](equity_ui.c) (the screen)
- Host: [`host/dot_host/apps/equity.py`](../../../../host/dot_host/apps/equity.py)
