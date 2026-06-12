import spotify_client
from app import App


class SpotifyApp(App):
    PREFIX = "SPOTIFY:"

    def init(self):
        self._art_album_id = None
        self._pending_art = None
        return spotify_client.init()

    def poll(self, sp) -> "str | None":
        track, artist, is_playing, progress_ms, duration_ms, album_id, art_url = (
            spotify_client.get_now_playing(sp)
        )

        if not track:
            return None

        state = "▶" if is_playing else "⏸"
        cur = f"{progress_ms // 60000}:{(progress_ms // 1000) % 60:02d}"
        tot = f"{duration_ms // 60000}:{(duration_ms // 1000) % 60:02d}"
        print(f"Spotify {state}: {track} — {artist}  [{cur}/{tot}]")

        if album_id and album_id != self._art_album_id:
            if art_url:
                art = spotify_client.fetch_album_art_rgb565(art_url)
                if art:
                    rgb565, accent = art
                    size = spotify_client.ART_SIZE
                    self._pending_art = spotify_client.format_img_msg(rgb565, size, size, accent)
                    self._art_album_id = album_id
                # else: fetch failed — retry on next poll
            else:
                self._art_album_id = album_id

        return spotify_client.format_msg(track, artist, is_playing, progress_ms, duration_ms)

    def poll_binary(self, sp) -> list:
        if self._pending_art is None:
            return []
        blob = self._pending_art
        self._pending_art = None
        return [blob]

    def on_command(self, cmd: str, sp) -> None:
        print(f">>> Command: {cmd}")
        if cmd == "play_pause":
            spotify_client.play_pause(sp)
        elif cmd == "next":
            spotify_client.next_track(sp)
        elif cmd == "prev":
            spotify_client.prev_track(sp)
        else:
            print(f"    Unknown command: {cmd}")
