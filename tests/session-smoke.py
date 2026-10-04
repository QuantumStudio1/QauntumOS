#!/usr/bin/env python3
"""Boot the new account flow and verify setup, unlock, and poweroff."""

import os
from pathlib import Path
import selectors
import subprocess
import sys
import time


REPO = Path(__file__).resolve().parents[1]
iso = os.environ.get("QAUNTUM_ISO")
if iso:
    command = [
        os.environ.get("QAUNTUM_QEMU", "qemu-system-x86_64"),
        "-m", "1024", "-smp", "2", "-display", "none", "-monitor", "none",
        "-serial", "stdio", "-cdrom", iso, "-boot", "d", "-no-reboot",
    ]
    if os.environ.get("QAUNTUM_QEMU_SHARE"):
        command[1:1] = ["-L", os.environ["QAUNTUM_QEMU_SHARE"]]
    if os.environ.get("QAUNTUM_OVMF_CODE"):
        command += ["-drive", f"if=pflash,format=raw,readonly=on,file={os.environ['QAUNTUM_OVMF_CODE']}"]
else:
    command = ["bash", str(REPO / "tools/run-vm.sh"), "-no-reboot"]
process = subprocess.Popen(
    command,
    cwd=REPO, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT, env=os.environ.copy(),
)
selector = selectors.DefaultSelector()
selector.register(process.stdout, selectors.EVENT_READ)
output = bytearray()
cursor = 0
deadline = time.monotonic() + 120


def expect(message: bytes) -> None:
    global cursor
    while time.monotonic() < deadline:
        index = output.find(message, cursor)
        if index >= 0:
            cursor = index + len(message)
            return
        for key, _ in selector.select(timeout=1):
            chunk = os.read(key.fileobj.fileno(), 4096)
            if chunk:
                output.extend(chunk)
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
        if process.poll() is not None:
            break
    raise RuntimeError(f"Did not see {message!r}; QEMU status {process.poll()}")


def answer(prompt: bytes, response: bytes) -> None:
    expect(prompt)
    process.stdin.write(response + b"\n")
    process.stdin.flush()


try:
    expect(b"starting account and lock screen")
    if os.environ.get("QAUNTUM_EXISTING_PROFILE") != "1":
        answer(b"PROFILE NAME: ", b"player1")
        answer(b"PASSWORD: ", b"CorrectHorse123")
        answer(b"CONFIRM PASSWORD: ", b"CorrectHorse123")
        expect(b"Profile created. Locking screen.")
    else:
        expect(b"LOCK SCREEN")
    answer(b"PASSWORD: ", b"incorrect")
    expect(b"Incorrect profile or password.")
    answer(b"PASSWORD: ", b"CorrectHorse123")
    expect(b"Unlocked profile player1.")
    answer(b"1 GAMES 2 SETTINGS 3 ADD L LOCK P POWER: ", b"p")
    expect(b"reboot: Power down")
    process.wait(timeout=10)
    if process.returncode != 0:
        raise RuntimeError(f"QEMU exited with status {process.returncode}")
    print("\nAccount and lock screen smoke test passed")
finally:
    if process.poll() is None:
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
            process.wait()
