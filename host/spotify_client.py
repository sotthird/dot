import os
import time
from pathlib import Path

from dotenv import load_dotenv

load_dotenv(Path(__file__).parent / ".env")

CLIENT_ID = os.getenv("SPOTIFY_CLIENT_ID")
CLIENT_SECRET = os.getenv("SPOTIFY_CLIENT_SECRET")
REDIRECT_URI = os.getenv("SPOTIFY_REDIRECT_URI")


def init():
    try:
        import spotipy
        from spotipy.oauth2 import SpotifyOAuth

        sp = spotipy.Spotify(
            auth_manager=SpotifyOAuth(
                client_id=CLIENT_ID,
                client_secret=CLIENT_SECRET,
                redirect_uri=REDIRECT_URI,
                scope="user-read-currently-playing user-modify-playback-state",
            ),
            retries=3,  # auto-retry on 429/5xx
            backoff_factor=1,  # wait 1s, 2s, 4s between retries
        )
        return sp
    except ImportError:
        print("spotipy not installed — run: pip install spotipy")
        return None
    except Exception as e:
        print(f"Spotify auth failed: {e}")
        return None


def get_now_playing(sp):
    """Returns (track, artist, is_playing, progress_ms, duration_ms) or defaults."""
    if sp is None:
        return None, None, False, 0, 0
    try:
        result = sp.currently_playing()
        if result and result.get("item"):
            track = result["item"]["name"]
            artist = result["item"]["artists"][0]["name"]
            is_playing = result.get("is_playing", False)
            progress_ms = result.get("progress_ms", 0) or 0
            duration_ms = result["item"].get("duration_ms", 0) or 0
            return track, artist, is_playing, progress_ms, duration_ms
    except Exception as e:
        if hasattr(e, "http_status") and e.http_status == 429:
            retry_after = int(getattr(e, "headers", {}).get("Retry-After", 5))
            print(f"Rate limited — waiting {retry_after}s")
            time.sleep(retry_after)
        # any other error: return defaults silently
    return None, None, False, 0, 0


# LVGL's built-in Montserrat fonts only cover the basic ASCII range, so
# typographic punctuation Spotify sends (curly quotes, dashes, ellipsis)
# renders as a missing-glyph box on the display. Map those to ASCII.
_ASCII_PUNCTUATION = {
    "‘": "'",
    "’": "'",
    "“": '"',
    "”": '"',
    "–": "-",
    "—": "-",
    "…": "...",
}


def _clean(s: str) -> str:
    """Strip/normalize characters that would corrupt the protocol or can't be displayed."""
    for unicode_char, ascii_char in _ASCII_PUNCTUATION.items():
        s = s.replace(unicode_char, ascii_char)
    return s.replace("|", " ").replace("\n", " ").replace("\r", " ")


def format_msg(
    track: str, artist: str, is_playing: bool, progress_ms: int, duration_ms: int
) -> str:
    return (
        f"SPOTIFY:{_clean(track)}|{_clean(artist)}|"
        f"{1 if is_playing else 0}|{progress_ms}|{duration_ms}\n"
    )


def play_pause(sp):
    if sp is None:
        return
    try:
        result = sp.currently_playing()
        if result and result.get("is_playing"):
            sp.pause_playback()
        else:
            sp.start_playback()
    except Exception as e:
        print(f"play_pause error: {e}")


def next_track(sp):
    if sp is None:
        return
    try:
        sp.next_track()
    except Exception as e:
        print(f"next_track error: {e}")


def prev_track(sp):
    if sp is None:
        return
    try:
        sp.previous_track()
    except Exception as e:
        print(f"prev_track error: {e}")
