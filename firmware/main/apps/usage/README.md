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

## Protocol

```
US:S|<today>|<5h>|<7d>|<messages today>|<5h %>|<7d %>|<5h reset min>|<7d reset min>
```

The token counts are already abbreviated by the host. The percentages are whole numbers and the
reset times are minutes; `-1` means unknown. For example `US:S|894.2k|557.5k|2.6M|104|28|9|295|9000`.
The device also accepts the shorter older forms. It sends nothing back.

## Files

- Firmware: [`usage.c`](usage.c) (parsing), [`usage_ui.c`](usage_ui.c) (the screen)
- Host: [`host/dot_host/apps/usage.py`](../../../../host/dot_host/apps/usage.py)
