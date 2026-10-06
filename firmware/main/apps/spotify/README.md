# spotify

What's playing on Spotify: track, artist, album art with a matching accent color, elapsed and total
time, and prev / play-pause / next buttons.

**Touch:** the three buttons control playback on whichever device is playing.

## Host

Needs the `spotify` extra (spotipy, pillow, ...) and a Spotify developer app.

1. Create an app in the [Spotify developer dashboard](https://developer.spotify.com/dashboard)
   and add a redirect URI to it.
2. Copy `host/.env.example` to `host/.env` and fill in `SPOTIFY_CLIENT_ID`,
   `SPOTIFY_CLIENT_SECRET` and `SPOTIFY_REDIRECT_URI`. The redirect URI must match the dashboard
   exactly.
3. Run it:

```bash
pip install -e ".[spotify]"
dot-host spotify
```

On first run a browser opens to authorize the app, and the token is cached in `host/.cache`. Both
`.env` and `.cache` are git-ignored. The app asks for the `user-read-currently-playing` and
`user-modify-playback-state` scopes.

## Protocol

```
SPOTIFY:<track>|<artist>|<playing 0/1>|<progress_ms>|<duration_ms>
IMG:<w>x<h>:<bytes>:<rrggbb>      followed by <bytes> of raw little-endian RGB565
```

The `IMG:` frame carries the album art, 180×180, and the accent color for the buttons and progress.
It is only sent when the album changes. Text is ASCII only: typographic quotes and dashes are mapped
to plain ones, and `|` is dropped.

The device sends `CMD:play_pause`, `CMD:next` and `CMD:prev`, which the host turns into Spotify API
calls.

## Files

- Firmware: [`spotify.c`](spotify.c) (parsing and art handoff), [`spotify_ui.c`](spotify_ui.c)
  (the screen)
- Host: [`host/dot_host/apps/spotify.py`](../../../../host/dot_host/apps/spotify.py)

## Troubleshooting

- **HTTP 429:** the host already waits out `Retry-After`. If it keeps happening, raise
  `POLL_INTERVAL` in `host/dot_host/cli.py`.
- **Nothing shows:** Spotify only reports a track while something is playing or paused on one of
  your devices.
- **Art does not appear:** the host's `ART_SIZE` must match the firmware's image buffer.
