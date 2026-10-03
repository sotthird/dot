"""The host half of the serial protocol: what each app puts on the wire."""

from dot_host.apps import ci, cpu, spotify


def test_cpu_message():
    assert cpu.format_msg(42.345) == "CPU:42.3\n"


def test_ci_message():
    status = ci.CiStatus("running", "me/repo", "fix bug", "CI / main", "#7  1/3 test")
    assert status.message() == "CI:running|me/repo|fix bug|CI / main|#7  1/3 test\n"


def test_ci_text_is_ascii_and_free_of_delimiters():
    assert ci.ascii_only("fix: café | naïve\nline") == "fix: caf   nave line"


def test_ci_run_info_skips_missing_parts():
    assert ci.run_info(129, "2m ago") == "#129  2m ago"
    assert ci.run_info(129, "") == "#129"
    assert ci.run_info(None, "just now") == "just now"
    assert ci.run_info(None, "") == ""


def test_ci_delta_is_compact():
    assert ci.format_delta("", running=False) == ""
    assert ci.format_delta("not a timestamp", running=False) == ""


def test_spotify_message_normalizes_text():
    msg = spotify.format_msg("Don’t | Stop", "A—B", True, 1000, 200000)
    assert msg == "SPOTIFY:Don't   Stop|A-B|1|1000|200000\n"


def test_spotify_image_header():
    blob = spotify.format_img_msg(b"\x01\x02", 180, 180, (255, 0, 16))
    assert blob == b"IMG:180x180:2:ff0010\n\x01\x02"


def test_spotify_picks_smallest_art_that_fits():
    images = [
        {"url": "big", "width": 640},
        {"url": "mid", "width": 300},
        {"url": "tiny", "width": 64},
    ]
    assert spotify.pick_art_url(images) == "mid"
    assert spotify.pick_art_url([{"url": "tiny", "width": 64}]) == "tiny"
    assert spotify.pick_art_url([]) is None
