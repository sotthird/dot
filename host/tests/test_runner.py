"""Only the app on the device's screen runs; the others are on standby."""

from dot_host.apps import App
from dot_host.runner import Runner


class Fake(App):
    def __init__(self, name: str):
        self.name = name
        self.polls = 0
        self.activations = 0
        self.commands: list[str] = []

    def poll(self):
        self.polls += 1
        return [f"{self.name}\n"]

    def on_activate(self):
        self.activations += 1

    def on_command(self, cmd):
        self.commands.append(cmd)


def make():
    a, b = Fake("a"), Fake("b")
    return a, b, Runner({"a": a, "b": b})


def test_nothing_runs_until_the_device_reports_its_app():
    a, b, runner = make()
    assert not runner.known
    assert runner.poll() == []
    assert a.polls == b.polls == 0


def test_only_the_active_app_is_polled():
    a, b, runner = make()
    runner.device_showing("a")
    assert runner.poll() == ["a\n"]
    assert runner.poll() == ["a\n"]
    assert (a.polls, b.polls) == (2, 0)


def test_switching_hands_over_and_activates_once():
    a, b, runner = make()
    runner.device_showing("a")
    runner.poll()
    runner.device_showing("b")
    assert runner.poll() == ["b\n"]
    runner.poll()
    assert (a.polls, b.polls) == (1, 2)
    assert (a.activations, b.activations) == (1, 1)


def test_commands_go_to_the_active_app():
    a, b, runner = make()
    runner.device_showing("b")
    runner.on_command("tap")
    assert (a.commands, b.commands) == ([], ["tap"])


def test_an_app_the_host_does_not_run_is_ignored():
    a, _, runner = make()
    runner.device_showing("spotify")
    assert runner.poll() == []
    runner.on_command("tap")
    assert a.polls == 0 and a.commands == []
