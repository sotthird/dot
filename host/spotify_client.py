import colorsys
import os
import sys
import time
from array import array
from io import BytesIO
from pathlib import Path

import requests
from dotenv import load_dotenv
from PIL import Image

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


ART_SIZE = 180


def _pick_art_url(images):
    """Pick the smallest album art image that's still >= ART_SIZE, else the smallest available."""
    if not images:
        return None
    candidates = sorted(images, key=lambda img: img.get("width") or 0)
    for img in candidates:
        if (img.get("width") or 0) >= ART_SIZE:
            return img["url"]
    return candidates[0]["url"]


def get_now_playing(sp):
    """Returns (track, artist, is_playing, progress_ms, duration_ms, album_id, art_url) or defaults."""
    if sp is None:
        return None, None, False, 0, 0, None, None
    try:
        result = sp.currently_playing()
        if result and result.get("item"):
            track = result["item"]["name"]
            artist = result["item"]["artists"][0]["name"]
            is_playing = result.get("is_playing", False)
            progress_ms = result.get("progress_ms", 0) or 0
            duration_ms = result["item"].get("duration_ms", 0) or 0
            album = result["item"].get("album", {})
            album_id = album.get("id")
            art_url = _pick_art_url(album.get("images", []))
            return track, artist, is_playing, progress_ms, duration_ms, album_id, art_url
    except Exception as e:
        if hasattr(e, "http_status") and e.http_status == 429:
            retry_after = int(getattr(e, "headers", {}).get("Retry-After", 5))
            print(f"Rate limited — waiting {retry_after}s")
            time.sleep(retry_after)
        # any other error: return defaults silently
    return None, None, False, 0, 0, None, None


def extract_accent_color(img: "Image.Image") -> tuple[int, int, int]:
    """Pick a vibrant accent color representative of the image, suited for a dark UI."""
    small = img.convert("RGB").resize((50, 50))
    paletted = small.quantize(colors=5, method=Image.Quantize.MEDIANCUT)
    palette = paletted.getpalette()
    counts = sorted(paletted.getcolors(), reverse=True)

    hsv = None
    for _count, idx in counts:
        r, g, b = palette[idx * 3 : idx * 3 + 3]
        h, s, v = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)
        if s >= 0.25 and 0.2 <= v <= 0.95:
            hsv = (h, s, v)
            break
    if hsv is None:
        _count, idx = counts[0]
        r, g, b = palette[idx * 3 : idx * 3 + 3]
        hsv = colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)

    h, s, v = hsv
    s = max(s, 0.45)
    v = max(min(v, 0.9), 0.5)
    r, g, b = colorsys.hsv_to_rgb(h, s, v)
    return int(r * 255), int(g * 255), int(b * 255)


def fetch_album_art_rgb565(
    url: str, size: int = ART_SIZE
) -> "tuple[bytes, tuple[int, int, int]] | None":
    """Download album art, returning (raw little-endian RGB565 pixel data, accent RGB color)."""
    try:
        resp = requests.get(url, timeout=5)
        resp.raise_for_status()
        img = Image.open(BytesIO(resp.content)).convert("RGB").resize((size, size))
        accent = extract_accent_color(img)
        pixels = img.getdata()
        rgb565 = array(
            "H", (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3) for r, g, b in pixels)
        )
        if sys.byteorder != "little":
            rgb565.byteswap()
        return rgb565.tobytes(), accent
    except Exception as e:
        print(f"Album art fetch failed: {e}")
        return None


def format_img_msg(rgb565: bytes, w: int, h: int, color: tuple[int, int, int]) -> bytes:
    r, g, b = color
    return f"IMG:{w}x{h}:{len(rgb565)}:{r:02x}{g:02x}{b:02x}\n".encode() + rgb565


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
