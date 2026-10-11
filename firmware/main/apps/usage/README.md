# usage

Claude Code usage, laid out like the Clawdmeter usage screen: a line with today's tokens and
message count, then a card each for the last 5 hours and the last 7 days. A card shows your real
plan utilization as a percentage, a rounded bar that turns amber from 50% and red from 80%, and
when the window resets ("Resets in 4h 55m").

No touch interaction. Off by default: turn it on under *Dot: Apps* in menuconfig.

## Host

No extra packages and no setup beyond being signed in to Claude Code on the same computer.

```bash
dot-host usage
```

The host reads Claude Code's login token from `~/.claude/.credentials.json` (or under
`$CLAUDE_CONFIG_DIR`) and, once a minute, sends one 1-token request to `api.anthropic.com`. The
reply's `anthropic-ratelimit-unified-5h/7d-*` headers carry your utilization and reset times, which
is how Clawdmeter reads them. The token is sent only to Anthropic, is never printed, and is never
refreshed by dot (Claude Code renews it; if it has expired, use Claude Code once). The request
counts against your plan, though only barely. On macOS the token lives in the Keychain, not in a
file, so this is not supported there yet. Accounts without 5-hour/7-day windows (Enterprise)
fall back as below.

The token counts come from the transcripts in `~/.claude/projects`, rescanned every 30 seconds.
Each message is counted once; input, output and cache-creation tokens are summed, and cache reads
are left out because they re-read the same context. "Today" is the host's local calendar day.

**Fallback.** When the utilization cannot be read (no login on this computer, expired token,
offline), the percentages are measured against budgets you set in `host/.env` (or the
environment; `python-dotenv` is read if installed), and no reset times are shown:

```
CLAUDE_5H_LIMIT=2000000
CLAUDE_7D_LIMIT=30000000
```

With neither, a card shows the token count and "no usage data".

## Standalone (no host)

For a device that works without `dot-host` running. On a Wi-Fi board (AMOLED) it is on by default.
Give the device its network and token once, over USB:

```bash
pio run -e amoled_2_16 -t upload     # flash as usual
dot-host setup                       # Wi-Fi name, password, Claude token (hidden); add --debug to watch the device log
```

It first asks whether you already have a token and, if not, offers to run `claude setup-token`
for you. It prints each step (secrets only by length). The device saves them in its flash and restarts. From then on, once a minute, it sends the same
1-token request as the host does and shows the 5-hour and 7-day percentages and reset times, using
the reply's `Date` header as its clock (no SNTP). Run `dot-host setup` again to change them; a blank
answer keeps the current value.

- Make the token with `claude setup-token` on any computer where you are signed in (it starts with
  `sk-ant-oat01-`). Do not copy `.credentials.json`: that token expires within hours and refreshing
  it from the device would log Claude Code out.
- Nothing secret is in the firmware image, the build or the repository. The values sit unencrypted
  in the device's flash, so treat the device like a password.
- Token counts and the message count need the transcripts, so they are not shown; the title reads
  "Claude plan usage".
- If `dot-host usage` is also running, its fuller data is shown and the device stays quiet; it
  fetches again two minutes after the last host message.
- The Wi-Fi name and password under *Dot: Radios* in menuconfig still work as defaults when
  nothing has been set with `dot-host setup`.

## Protocol

```
US:S|<today>|<5h>|<7d>|<messages today>|<5h %>|<7d %>|<5h reset min>|<7d reset min>
```

The token counts are already abbreviated by the host. The percentages are whole numbers and the
reset times are minutes; `-1` means unknown. For example `US:S|894.2k|557.5k|2.6M|104|28|9|295|9000`.
Standalone mode feeds the same line to itself with `--` for the token counts. The device also accepts the shorter older forms. It sends nothing back.

## Files

- Firmware: [`usage.c`](usage.c) (parsing), [`usage_standalone.c`](usage_standalone.c) (on-device fetch), [`usage_ui.c`](usage_ui.c) (the screen)
- Host: [`host/dot_host/apps/usage.py`](../../../../host/dot_host/apps/usage.py)

`dot-host setup` uses a separate set of lines, handled by the firmware whichever app is on screen:
`CFG:<wifi_ssid|wifi_pass|claude_tok>|<value>` stores a value (the device answers `CFG:OK` or
`CFG:ERR`), and `CFG:REBOOT` restarts it.
