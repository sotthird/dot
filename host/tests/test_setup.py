import pytest

from dot_host import setup


class FakeSerial:
    port = "fake"

    def __init__(self, replies):
        self.written = []
        self.replies = list(replies)

    def write(self, data):
        self.written.append(data)

    def flush(self):
        pass

    def readline(self):
        return (self.replies.pop(0) if self.replies else "").encode()


def test_apply_sends_each_setting_then_reboot():
    ser = FakeSerial(["CFG:OK\n", "CFG:OK\n"])
    setup.apply(ser, {"wifi_ssid": "home", "claude_tok": "sk-ant-oat01-x"})
    assert ser.written == [
        b"CFG:wifi_ssid|home\n",
        b"CFG:claude_tok|sk-ant-oat01-x\n",
        b"CFG:REBOOT\n",
    ]


def test_password_with_a_pipe_is_sent_whole():
    ser = FakeSerial(["CFG:OK\n"])
    setup.send_setting(ser, "wifi_pass", "a|b")
    assert ser.written == [b"CFG:wifi_pass|a|b\n"]


def test_rejected_setting_raises():
    with pytest.raises(setup.SetupError, match="rejected"):
        setup.send_setting(FakeSerial(["CFG:ERR\n"]), "wifi_ssid", "x")


def test_no_answer_raises(monkeypatch):
    monkeypatch.setattr(setup, "REPLY_TIMEOUT", 0.05)
    with pytest.raises(setup.SetupError, match="no answer"):
        setup.send_setting(FakeSerial([]), "wifi_ssid", "x")


def test_line_break_in_value_is_refused():
    with pytest.raises(setup.SetupError):
        setup.send_setting(FakeSerial([]), "claude_tok", "a\nCFG:REBOOT")


def test_progress_hides_secrets(capsys):
    ser = FakeSerial(["CFG:OK\n", "CFG:OK\n", "CFG:OK\n"])
    setup.apply(ser, {"wifi_ssid": "home", "wifi_pass": "hunter2", "claude_tok": "sk-ant-oat01-x"})
    out = capsys.readouterr().out
    assert "CFG:wifi_ssid|home" in out
    assert "hunter2" not in out and "sk-ant" not in out
    assert "<7 characters>" in out and "<- CFG:OK" in out


def test_token_prompt_skips_generation_when_user_has_one(monkeypatch):
    answers = iter(["y"])
    monkeypatch.setattr("builtins.input", lambda _: next(answers))
    ran = []
    monkeypatch.setattr(setup.subprocess, "run", lambda *a, **k: ran.append(a))
    assert setup.get_token(read=lambda _: "sk-ant-oat01-abc") == "sk-ant-oat01-abc"
    assert ran == []


def test_token_prompt_offers_to_generate_otherwise(monkeypatch):
    answers = iter(["n", "y"])
    monkeypatch.setattr("builtins.input", lambda _: next(answers))
    monkeypatch.setattr(setup.shutil, "which", lambda _: "/usr/bin/claude")
    ran = []
    monkeypatch.setattr(setup.subprocess, "run", lambda *a, **k: ran.append(a[0]))
    setup.get_token(read=lambda _: "tok")
    assert ran == [["claude", "setup-token"]]


def test_wait_ready_skips_boot_log_until_the_device_answers():
    ser = FakeSerial(["rst:0x15\n", "I (24) boot: ...\n", "", "APP:spotify\n"])
    setup.wait_ready(ser, say=lambda _: None)
    assert ser.written.count(b"HELLO\n") == 2


def test_wait_ready_gives_up(monkeypatch):
    monkeypatch.setattr(setup, "READY_TIMEOUT", 0.05)
    with pytest.raises(setup.SetupError, match="not answering"):
        setup.wait_ready(FakeSerial([]), say=lambda _: None)
