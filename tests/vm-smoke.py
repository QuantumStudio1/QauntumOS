#!/usr/bin/env python3
"""Boot QauntumOS in QEMU and check its kernel and recovery shell."""

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
proc = subprocess.Popen(
    command,
    cwd=REPO,
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    env=os.environ.copy(),
)
selector = selectors.DefaultSelector()
selector.register(proc.stdout, selectors.EVENT_READ)
output = bytearray()
stage = 0
deadline = time.monotonic() + 120

try:
    while time.monotonic() < deadline:
        for key, _ in selector.select(timeout=1):
            chunk = os.read(key.fileobj.fileno(), 4096)
            if chunk:
                output.extend(chunk)
                sys.stdout.buffer.write(chunk)
                sys.stdout.buffer.flush()
        if stage == 0 and b"QauntumOS boot ready" in output:
            proc.stdin.write(b"uname\n")
            proc.stdin.flush()
            stage = 1
        elif stage == 1 and b"Linux 7.2.8 x86_64" in output:
            proc.stdin.write(b"version\n")
            proc.stdin.flush()
            stage = 2
        elif stage == 2 and b"QauntumOS Version 1 boot prototype" in output:
            proc.stdin.write(b"poweroff\n")
            proc.stdin.flush()
            stage = 3
        if proc.poll() is not None:
            break
    else:
        raise RuntimeError("VM boot timed out")

    if b"QauntumOS boot ready" not in output:
        raise RuntimeError("QauntumOS shell did not start")
    if b"Linux 7.2.8 x86_64" not in output:
        raise RuntimeError("Running kernel version was not confirmed")
    if b"QauntumOS Version 1 boot prototype" not in output:
        raise RuntimeError("Recovery shell did not answer the version command")
    if proc.returncode != 0:
        raise RuntimeError(f"QEMU exited with status {proc.returncode}")
    print("\nVM smoke test passed")
finally:
    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=5)
        except subprocess.TimeoutExpired:
            proc.kill()
            proc.wait()
