import spotify_client
from app import App


class SpotifyApp(App):
    PREFIX = "SPOTIFY:"

    def init(self):
        return spotify_client.init()

    def poll(self, sp) -> "str | None":
        track, artist, is_playing, progress_ms, duration_ms = spotify_client.get_now_playing(sp)

        if not track:
            return None

        state = "▶" if is_playing else "⏸"
        cur = f"{progress_ms // 60000}:{(progress_ms // 1000) % 60:02d}"
        tot = f"{duration_ms // 60000}:{(duration_ms // 1000) % 60:02d}"
        print(f"Spotify {state}: {track} — {artist}  [{cur}/{tot}]")

        return spotify_client.format_msg(track, artist, is_playing, progress_ms, duration_ms)

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
