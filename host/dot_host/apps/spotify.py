"""Spotify "Now Playing": track, progress and album art, plus on-device transport controls."""

import colorsys
import os
import sys
import time
from array import array
from io import BytesIO
from typing import cast

import requests
import spotipy
from dotenv import load_dotenv
from PIL import Image
from spotipy.oauth2 import SpotifyOAuth

from .base import App

ART_SIZE = 180  # must match the firmware's image buffer

# LVGL's built-in Montserrat fonts only cover the basic ASCII range, so typographic
# punctuation Spotify sends (curly quotes, dashes, ellipsis) would render as a
# missing-glyph box on the display. Map those to ASCII.
ASCII_PUNCTUATION = {
    "‘": "'",
    "’": "'",
    "“": '"',
    "”": '"',
    "–": "-",
    "—": "-",
    "…": "...",
}


def clean(s: str) -> str:
    """Normalize characters that cannot be displayed or would corrupt the protocol."""
    for unicode_char, ascii_char in ASCII_PUNCTUATION.items():
        s = s.replace(unicode_char, ascii_char)
    return s.replace("|", " ").replace("\n", " ").replace("\r", " ")


def format_msg(
    track: str, artist: str, is_playing: bool, progress_ms: int, duration_ms: int
) -> str:
    return (
        f"SPOTIFY:{clean(track)}|{clean(artist)}|"
        f"{1 if is_playing else 0}|{progress_ms}|{duration_ms}\n"
    )


def format_img_msg(rgb565: bytes, w: int, h: int, color: tuple[int, int, int]) -> bytes:
    r, g, b = color
    return f"IMG:{w}x{h}:{len(rgb565)}:{r:02x}{g:02x}{b:02x}\n".encode() + rgb565


def pick_art_url(images: list[dict]) -> str | None:
    """Pick the smallest album art image that is still >= ART_SIZE, else the smallest."""
    if not images:
        return None
    candidates = sorted(images, key=lambda img: img.get("width") or 0)
    for img in candidates:
        if (img.get("width") or 0) >= ART_SIZE:
            return img["url"]
    return candidates[0]["url"]


def _palette_hsv(palette: list[int], idx: int) -> tuple[float, float, float]:
    r, g, b = palette[idx * 3 : idx * 3 + 3]
    return colorsys.rgb_to_hsv(r / 255, g / 255, b / 255)


def extract_accent_color(img: Image.Image) -> tuple[int, int, int]:
    """Pick a vibrant accent color representative of the image, suited for a dark UI."""
    small = img.convert("RGB").resize((50, 50))
    paletted = small.quantize(colors=5, method=Image.Quantize.MEDIANCUT)
    palette = paletted.getpalette() or []
    counts = sorted(cast(list[tuple[int, int]], paletted.getcolors()), reverse=True)

    # Most common color that is neither washed out nor too dark/bright; else the most common
    candidates = [_palette_hsv(palette, idx) for _count, idx in counts]
    h, s, v = next((c for c in candidates if c[1] >= 0.25 and 0.2 <= c[2] <= 0.95), candidates[0])

    s = max(s, 0.45)
    v = max(min(v, 0.9), 0.5)
    red, green, blue = colorsys.hsv_to_rgb(h, s, v)
    return int(red * 255), int(green * 255), int(blue * 255)


def fetch_album_art_rgb565(
    url: str, size: int = ART_SIZE
) -> tuple[bytes, tuple[int, int, int]] | None:
    """Download album art, returning (little-endian RGB565 pixel data, accent RGB color)."""
    try:
        resp = requests.get(url, timeout=5)
        resp.raise_for_status()
        img = Image.open(BytesIO(resp.content)).convert("RGB").resize((size, size))
        accent = extract_accent_color(img)
        raw = img.tobytes()  # packed R, G, B bytes
        rgb565 = array(
            "H",
            (
                ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
                for r, g, b in zip(raw[0::3], raw[1::3], raw[2::3], strict=True)
            ),
        )
        if sys.byteorder != "little":
            rgb565.byteswap()
        return rgb565.tobytes(), accent
    except Exception as e:
        print(f"Album art fetch failed: {e}")
        return None


def play_pause(sp: spotipy.Spotify) -> None:
    playing = sp.currently_playing()
    if playing and playing.get("is_playing"):
        sp.pause_playback()
    else:
        sp.start_playback()


class SpotifyApp(App):
    def __init__(self) -> None:
        self._sp: spotipy.Spotify | None = None
        self._art_album_id: str | None = None

    def start(self) -> None:
        load_dotenv()  # SPOTIFY_CLIENT_ID / _SECRET / REDIRECT_URI, from host/.env
        try:
            self._sp = spotipy.Spotify(
                auth_manager=SpotifyOAuth(
                    client_id=os.getenv("SPOTIFY_CLIENT_ID"),
                    client_secret=os.getenv("SPOTIFY_CLIENT_SECRET"),
                    redirect_uri=os.getenv("SPOTIFY_REDIRECT_URI"),
                    scope="user-read-currently-playing user-modify-playback-state",
                ),
                retries=3,  # auto-retry on 429/5xx
                backoff_factor=1,  # wait 1s, 2s, 4s between retries
            )
        except Exception as e:
            print(f"Spotify auth failed: {e}")

    def _now_playing(self):
        """The current playback item and its state, or None if nothing is playing."""
        if self._sp is None:
            return None
        try:
            result = self._sp.currently_playing()
        except Exception as e:
            if getattr(e, "http_status", None) == 429:
                retry_after = int(getattr(e, "headers", {}).get("Retry-After", 5))
                print(f"Rate limited - waiting {retry_after}s")
                time.sleep(retry_after)
            return None  # any other error: skip this cycle silently
        return result if result and result.get("item") else None

    def poll(self) -> list[str | bytes]:
        result = self._now_playing()
        if result is None:
            return []

        item = result["item"]
        track = item["name"]
        artist = item["artists"][0]["name"]
        is_playing = result.get("is_playing", False)
        progress_ms = result.get("progress_ms", 0) or 0
        duration_ms = item.get("duration_ms", 0) or 0

        cur = f"{progress_ms // 60000}:{(progress_ms // 1000) % 60:02d}"
        tot = f"{duration_ms // 60000}:{(duration_ms // 1000) % 60:02d}"
        print(f"Spotify {'>' if is_playing else '||'}: {track} - {artist}  [{cur}/{tot}]")

        messages: list[str | bytes] = [
            format_msg(track, artist, is_playing, progress_ms, duration_ms)
        ]

        album = item.get("album", {})
        album_id = album.get("id")
        if album_id and album_id != self._art_album_id:
            art_url = pick_art_url(album.get("images", []))
            if art_url is None:
                self._art_album_id = album_id
            elif art := fetch_album_art_rgb565(art_url):
                rgb565, accent = art
                messages.append(format_img_msg(rgb565, ART_SIZE, ART_SIZE, accent))
                self._art_album_id = album_id
            # else: the fetch failed; retry on the next poll
        return messages

    def on_command(self, cmd: str) -> None:
        print(f">>> Command: {cmd}")
        sp = self._sp
        if sp is None:
            return
        actions = {
            "play_pause": lambda: play_pause(sp),
            "next": sp.next_track,
            "prev": sp.previous_track,
        }
        action = actions.get(cmd)
        if action is None:
            print(f"    Unknown command: {cmd}")
            return
        try:
            action()
        except Exception as e:
            print(f"{cmd} error: {e}")
