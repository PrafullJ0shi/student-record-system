#!/usr/bin/env python3
"""Record a real srms session to an asciinema v2 cast."""

from __future__ import annotations

import json
import os
import pty
import select
import struct
import subprocess
import termios
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CAST = Path("/tmp/srms-demo.cast")
COLS = 96
ROWS = 36


def set_winsize(fd: int, rows: int, cols: int) -> None:
    packed = struct.pack("HHHH", rows, cols, 0, 0)
    import fcntl

    fcntl.ioctl(fd, termios.TIOCSWINSZ, packed)


def main() -> None:
    master, slave = pty.openpty()
    set_winsize(slave, ROWS, COLS)
    env = os.environ.copy()
    env["TERM"] = "xterm-256color"
    env["LANG"] = "C.UTF-8"
    env.pop("NO_COLOR", None)
    proc = subprocess.Popen(
        [str(ROOT / "build" / "srms"), "--color", "--file", "data/sample_cohort.srms"],
        stdin=slave,
        stdout=slave,
        stderr=slave,
        env=env,
        cwd=str(ROOT),
        close_fds=True,
    )
    os.close(slave)

    start = time.monotonic()
    events: list[list] = []
    pending = ""

    def stamp() -> float:
        return round(time.monotonic() - start, 3)

    def drain(timeout: float) -> None:
        nonlocal pending
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            remaining = deadline - time.monotonic()
            ready, _, _ = select.select([master], [], [], min(0.05, max(0.0, remaining)))
            if not ready:
                return
            try:
                data = os.read(master, 65536)
            except OSError:
                return
            if not data:
                return
            text = data.decode("utf-8", errors="replace")
            events.append([stamp(), "o", text])
            pending += text

    def wait_prompt(since: int, timeout: float = 6.0) -> None:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            drain(0.12)
            tail = pending[since:]
            if tail.endswith("> ") or tail.endswith(">"):
                return
        raise SystemExit("timed out waiting for a prompt:\n" + pending[since:][-500:])

    def send(text: str, per: float = 0.08) -> None:
        for ch in text:
            os.write(master, ch.encode())
            time.sleep(per)
            drain(0.03)

    def hold(seconds: float) -> None:
        end = time.monotonic() + seconds
        while time.monotonic() < end:
            drain(min(0.2, end - time.monotonic()))

    # Hide the cursor in the recording only. The bytes are part of the cast,
    # not a change to what an operator's terminal does after exit.
    events.append([0.0, "o", "\033[?25l"])
    marks: dict[str, float] = {}
    wait_prompt(0)
    marks["menu"] = stamp()
    hold(1.6)

    mark = len(pending)
    send("3\n", 0.12)
    wait_prompt(mark)
    marks["register"] = stamp()
    hold(4.0)

    mark = len(pending)
    send("\n")
    wait_prompt(mark)
    marks["menu2"] = stamp()
    hold(1.0)

    mark = len(pending)
    send("7\n", 0.12)
    wait_prompt(mark)
    marks["statistics"] = stamp()
    hold(4.6)

    mark = len(pending)
    send("\n")
    wait_prompt(mark)
    hold(0.8)

    mark = len(pending)
    send("5\n", 0.12)
    wait_prompt(mark)
    mark = len(pending)
    send("Iyer\n", 0.11)
    wait_prompt(mark)
    marks["search"] = stamp()
    hold(3.2)

    mark = len(pending)
    send("\n")
    wait_prompt(mark)
    marks["menu_end"] = stamp()
    hold(1.8)

    header = {
        "version": 2,
        "width": COLS,
        "height": ROWS,
        "timestamp": int(time.time()),
        "env": {"TERM": "xterm-256color", "SHELL": "/bin/bash"},
        "title": "srms sample cohort",
    }
    with CAST.open("w", encoding="utf-8") as handle:
        handle.write(json.dumps(header) + "\n")
        for event in events:
            handle.write(json.dumps(event, ensure_ascii=False) + "\n")

    proc.terminate()
    try:
        proc.wait(timeout=2)
    except subprocess.TimeoutExpired:
        proc.kill()
    os.close(master)
    print(f"cast {CAST}")
    for name, when in marks.items():
        print(f"mark {name} {when:.2f}")


if __name__ == "__main__":
    main()
