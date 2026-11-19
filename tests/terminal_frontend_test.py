#!/usr/bin/env python3
"""Bounded PTY checks for the production Gamebloke terminal frontend."""

from __future__ import annotations

import fcntl
import os
import pty
import select
import re
import signal
import struct
import subprocess
import sys
import tempfile
import termios
import time
from pathlib import Path


GREEN = (0, 255, 0)
BLUE = (0, 0, 255)
RED = (255, 0, 0)
MAX_WAIT_SECONDS = 5.0


class Screen:
    """Small ANSI SGR/cursor reader for the host's colored ASCII view."""

    def __init__(self) -> None:
        self.pending = bytearray()
        self.row = 1
        self.column = 1
        self.background: tuple[int, int, int] | None = None
        self.cells: dict[tuple[int, int], tuple[int, int, int] | None] = {}
        self.text = bytearray()

    def feed(self, data: bytes) -> None:
        self.pending.extend(data)
        index = 0
        while index < len(self.pending):
            byte = self.pending[index]
            if byte == 0x1B:
                if index + 1 >= len(self.pending):
                    break
                if self.pending[index + 1] != ord("["):
                    index += 2
                    continue
                end = index + 2
                while end < len(self.pending) and not (0x40 <= self.pending[end] <= 0x7E):
                    end += 1
                if end >= len(self.pending):
                    break
                self._csi(bytes(self.pending[index + 2 : end]), chr(self.pending[end]))
                index = end + 1
                continue
            if byte == 0x0D:
                self.column = 1
            elif byte == 0x0A:
                self.row += 1
                self.column = 1
            elif 0x20 <= byte <= 0x7E:
                self.text.append(byte)
                self.cells[(self.row, self.column)] = (
                    self.background if byte == ord(" ") else None
                )
                self.column += 1
            index += 1
        if index:
            del self.pending[:index]

    def _csi(self, raw_parameters: bytes, final: str) -> None:
        parameters = raw_parameters.decode("ascii", errors="ignore")
        if final in ("H", "f"):
            clean = parameters.lstrip("?")
            values = clean.split(";") if clean else []
            self.row = int(values[0] or "1") if values else 1
            self.column = int(values[1] or "1") if len(values) > 1 else 1
        elif final == "J" and parameters.endswith("2"):
            self.cells.clear()
        elif final == "m":
            values = [int(part) if part else 0 for part in parameters.split(";")]
            offset = 0
            while offset < len(values):
                value = values[offset]
                if value == 0:
                    self.background = None
                elif value == 48 and offset + 4 < len(values) and values[offset + 1] == 2:
                    self.background = tuple(values[offset + 2 : offset + 5])
                    offset += 4
                offset += 1

    def cell(self, source_x: int, source_y: int) -> tuple[int, int, int] | None:
        # The frontend displays source coordinates at four-by-eight sampling.
        terminal_row = 3 + source_y // 8
        terminal_column = 1 + source_x // 4
        return self.cells.get((terminal_row, terminal_column))

    def has_banner(self) -> bool:
        return b"Gamebloke Playable 0" in self.text


class Session:
    def __init__(self, executable: str, module: str, content: str, frames: int = 240,
                 size: tuple[int, int] = (24, 80),
                 term: str = "xterm-256color") -> None:
        self.master, self.slave = pty.openpty()
        self.output = os.open(
            os.ttyname(self.slave), os.O_RDWR | getattr(os, "O_NOCTTY", 0)
        )
        self.saved_attributes = termios.tcgetattr(self.slave)
        self.saved_flags = fcntl.fcntl(self.slave, fcntl.F_GETFL)
        self.saved_output_flags = fcntl.fcntl(self.output, fcntl.F_GETFL)
        rows, columns = size
        fcntl.ioctl(self.slave, termios.TIOCSWINSZ,
                    struct.pack("HHHH", rows, columns, 0, 0))
        self.process = subprocess.Popen(
            [executable, "--frames", str(frames), module, content],
            stdin=self.slave,
            stdout=self.output,
            stderr=self.output,
            close_fds=True,
            env={**os.environ, "TERM": term, "COLORTERM": "truecolor"},
        )
        self.screen = Screen()
        self.closed = False

    def send(self, data: bytes) -> None:
        os.write(self.master, data)

    def read_once(self, timeout: float = 0.1) -> None:
        if self.master < 0:
            time.sleep(timeout)
            return
        ready, _, _ = select.select([self.master], [], [], timeout)
        if not ready:
            return
        try:
            chunk = os.read(self.master, 65536)
        except OSError as error:
            if error.errno in (5, 6):
                return
            raise
        if chunk:
            self.screen.feed(chunk)

    def wait_for(self, predicate, description: str, timeout: float = MAX_WAIT_SECONDS) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if predicate():
                return
            if self.process.poll() is not None:
                break
            self.read_once(min(0.1, max(0.0, deadline - time.monotonic())))
        raise AssertionError(
            f"timed out waiting for {description}; rc={self.process.poll()} "
            f"marker64={self.screen.cell(64, 80)} "
            f"marker128={self.screen.cell(128, 80)} "
            f"colors={sorted(set(self.screen.cells.values()), key=str)} "
            f"text={self.screen.text[-160:]!r}"
        )

    def wait_exit(self, timeout: float = MAX_WAIT_SECONDS) -> int:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                return self.process.wait()
            self.read_once(min(0.1, max(0.0, deadline - time.monotonic())))
        raise AssertionError("terminal frontend did not exit within the bound")

    def assert_restored(self) -> None:
        attributes = termios.tcgetattr(self.slave)
        flags = fcntl.fcntl(self.slave, fcntl.F_GETFL)
        output_flags = fcntl.fcntl(self.output, fcntl.F_GETFL)
        expected_attributes = list(self.saved_attributes)
        restored_attributes = list(attributes)
        # Darwin may add PENDIN when switching back to canonical input so the
        # line discipline reprocesses pending PTY input. It is kernel state,
        # not an application terminal-mode change.
        pending_input = getattr(termios, "PENDIN", 0)
        expected_attributes[3] &= ~pending_input
        restored_attributes[3] &= ~pending_input
        if restored_attributes != expected_attributes:
            raise AssertionError("terminal attributes were not restored")
        if (flags & os.O_NONBLOCK) != (self.saved_flags & os.O_NONBLOCK):
            raise AssertionError("terminal nonblocking flag was not restored")
        if (output_flags & os.O_NONBLOCK) != (
            self.saved_output_flags & os.O_NONBLOCK
        ):
            raise AssertionError("terminal output nonblocking flag was not restored")

    def close(self, force: bool = False) -> None:
        if self.closed:
            return
        if force and self.process.poll() is None:
            try:
                self.process.kill()
            except ProcessLookupError:
                pass
            self.process.wait(timeout=2)
        for fd in (self.master, self.slave, self.output):
            if fd < 0:
                continue
            try:
                os.close(fd)
            except OSError:
                pass
        self.closed = True


def require_exists(path: str, description: str) -> None:
    if not Path(path).is_file():
        raise AssertionError(f"missing {description}: {path}")


def run_cli(executable: str, *args: str, expected_success: bool) -> subprocess.CompletedProcess[bytes]:
    result = subprocess.run(
        [executable, *args],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        timeout=MAX_WAIT_SECONDS,
        check=False,
        env={**os.environ, "TERM": "xterm-256color", "COLORTERM": "truecolor"},
    )
    if (result.returncode == 0) != expected_success:
        raise AssertionError(
            f"CLI {args!r} returned {result.returncode}; "
            f"stdout={result.stdout[-400:]!r} stderr={result.stderr[-400:]!r}"
        )
    return result


def start_session(executable: str, module: str, content: str,
                  size: tuple[int, int] = (24, 80),
                  marker_color: tuple[int, int, int] = GREEN) -> Session:
    session = Session(executable, module, content, size=size)
    try:
        session.wait_for(session.screen.has_banner, "Gamebloke banner")
        session.wait_for(
            lambda: session.screen.cell(64, 80) == marker_color,
            "neutral green marker at x=64",
        )
        attributes = termios.tcgetattr(session.slave)
        if attributes[3] & (termios.ECHO | termios.ICANON):
            raise AssertionError("terminal input was not put into noncanonical/no-echo mode")
        return session
    except Exception:
        session.close(force=True)
        raise


def run_live_tracer(executable: str, module: str, content: str) -> None:
    session = start_session(executable, module, content)
    try:
        if session.screen.cell(128, 80) == GREEN:
            raise AssertionError("neutral marker unexpectedly appears at x=128")
        session.send(b"d")
        session.wait_for(lambda: session.screen.cell(128, 80) == GREEN,
                         "RIGHT press moves the marker to x=128")
        session.send(b"j")
        session.wait_for(lambda: session.screen.cell(128, 80) == BLUE,
                         "A press changes the marker to blue while RIGHT remains held")
        session.send(b"f")
        session.wait_for(lambda: session.screen.cell(64, 80) == BLUE,
                         "RIGHT release moves the held-A marker back to x=64")
        session.send(b"k")
        session.wait_for(lambda: session.screen.cell(64, 80) == GREEN,
                         "A release restores the neutral green marker")
        session.send(b" ")
        session.send(b"r")
        session.wait_for(lambda: session.screen.cell(64, 80) == GREEN,
                         "reset returns to the neutral guest")
        session.send(b"q")
        result = session.wait_exit()
        if result != 0:
            raise AssertionError(f"Q exit returned {result}")
        session.assert_restored()
        samples = re.findall(
            rb"frame=([0-9]+)\s+output=([0-9]+\.[0-9]+) FPS",
            bytes(session.screen.text),
        )
        if not samples:
            raise AssertionError("terminal output did not include its bounded cadence display")
    finally:
        session.close(force=True)


def run_display_cadence(executable: str, module: str, content: str) -> None:
    session = Session(executable, module, content, frames=30)
    try:
        result = session.wait_exit()
        if result != 0:
            raise AssertionError(f"bounded display sample returned {result}")
        session.assert_restored()
        samples = re.findall(
            rb"frame=([0-9]+)\s+output=([0-9]+\.[0-9]+) FPS",
            bytes(session.screen.text),
        )
        if not samples:
            raise AssertionError("terminal display cadence sample was missing")
        frames, fps = samples[-1]
        measured_fps = float(fps)
        elapsed_ms = int(frames) * 1000.0 / measured_fps
        print(
            "METRIC terminal.display "
            f"frames={frames.decode()} elapsed_ms={elapsed_ms:.3f} "
            f"fps={measured_fps:.2f} sampling=240x160-to-60x20"
        )
    finally:
        session.close(force=True)


def run_negative_guests(executable: str, module: str, guest: Path) -> None:
    input_disabled = guest.with_name("owned-playable0-interactive-input-disabled.gba")
    wrong_color = guest.with_name("owned-playable0-interactive-wrong-color.gba")
    require_exists(str(input_disabled), "input-disabled guest control")
    require_exists(str(wrong_color), "wrong-color guest control")

    session = start_session(executable, module, str(input_disabled))
    try:
        session.send(b"d")
        time.sleep(0.25)
        while True:
            ready, _, _ = select.select([session.master], [], [], 0)
            if not ready:
                break
            session.read_once(0)
        if session.screen.cell(128, 80) == GREEN:
            raise AssertionError("input-disabled guest passed the RIGHT movement assertion")
        session.send(b"q")
        session.wait_exit()
        session.assert_restored()
    finally:
        session.close(force=True)

    session = start_session(executable, module, str(wrong_color),
                            marker_color=RED)
    try:
        if session.screen.cell(64, 80) == GREEN:
            raise AssertionError("wrong-color guest passed the neutral-color assertion")
        session.send(b"q")
        session.wait_exit()
        session.assert_restored()
    finally:
        session.close(force=True)


def run_lifecycle(executable: str, module: str, content: str) -> None:
    for exit_kind in ("ctrl-d", "ctrl-c", "sigterm", "sighup", "disconnect", "ctrl-z"):
        session = start_session(executable, module, content)
        try:
            if exit_kind == "ctrl-d":
                session.send(b"\x04")
                session.wait_exit()
            elif exit_kind == "ctrl-c":
                session.process.send_signal(signal.SIGINT)
                session.wait_exit()
            elif exit_kind == "sigterm":
                session.process.send_signal(signal.SIGTERM)
                session.wait_exit()
            elif exit_kind == "sighup":
                session.process.send_signal(signal.SIGHUP)
                session.wait_exit()
            elif exit_kind == "disconnect":
                os.close(session.master)
                session.master = -1
                session.wait_exit()
            else:
                session.process.send_signal(signal.SIGTSTP)
                time.sleep(0.2)
                if session.process.poll() is not None:
                    raise AssertionError("Ctrl-Z unexpectedly stopped/terminated the frontend")
                session.send(b"q")
                session.wait_exit()
            session.assert_restored()
        finally:
            session.close(force=True)

    resized = start_session(executable, module, content)
    try:
        fcntl.ioctl(resized.master, termios.TIOCSWINSZ,
                    struct.pack("HHHH", 10, 40, 0, 0))
        # This PTY child is not made a session leader, so Darwin does not
        # deliver SIGWINCH through a controlling terminal process group.
        resized.process.send_signal(signal.SIGWINCH)
        resized.wait_for(
            lambda: b"Terminal needs at least 60 columns by 24 rows" in resized.screen.text,
            "resize notice after shrinking the terminal",
        )
        resized.send(b"q")
        resized.wait_exit()
        resized.assert_restored()
    finally:
        resized.close(force=True)


def run_refusals(executable: str, module: str, content: str) -> None:
    run_cli(executable, "--check", module, content, expected_success=True)
    run_cli(executable, module, content, expected_success=False)
    run_cli(executable, "--unknown", module, content, expected_success=False)
    run_cli(executable, "--check", module + ".missing", content, expected_success=False)
    with tempfile.TemporaryDirectory(prefix="gamebloke-terminal-") as scratch:
        malformed = Path(scratch) / "short.gba"
        malformed.write_bytes(b"short")
        run_cli(executable, "--check", module, str(malformed),
                expected_success=False)

    too_small = Session(executable, module, content, frames=20, size=(10, 40))
    try:
        result = too_small.wait_exit()
        if result == 0:
            raise AssertionError("too-small terminal was accepted")
        too_small.assert_restored()
    finally:
        too_small.close(force=True)

    dumb = Session(executable, module, content, size=(24, 80), term="dumb")
    try:
        result = dumb.wait_exit()
        if result == 0:
            raise AssertionError("TERM=dumb was accepted for interactive rendering")
        dumb.assert_restored()
    finally:
        dumb.close(force=True)

    for frame_count in ("0", "1001", "-1", "not-a-number"):
        run_cli(executable, "--frames", frame_count, module, content,
                expected_success=False)


def run_tracer(executable: str, module: str, content: str) -> None:
    require_exists(executable, "terminal frontend executable")
    require_exists(module, "Gamebloke Libretro module")
    require_exists(content, "continuous Playable 0 guest")
    run_refusals(executable, module, content)
    run_live_tracer(executable, module, content)
    run_display_cadence(executable, module, content)
    run_negative_guests(executable, module, Path(content))
    run_lifecycle(executable, module, content)
    print("terminal-frontend-test: tracer, live controls, negative guests, and lifecycle passed")


def main() -> int:
    if len(sys.argv) != 5 or sys.argv[1] != "tracer":
        print(
            "usage: terminal_frontend_test.py tracer EXECUTABLE MODULE CONTENT",
            file=sys.stderr,
        )
        return 2
    try:
        run_tracer(sys.argv[2], sys.argv[3], sys.argv[4])
    except (AssertionError, OSError, subprocess.SubprocessError) as error:
        print(f"terminal-frontend-test: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
