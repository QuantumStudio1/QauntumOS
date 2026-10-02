#!/usr/bin/env python3
"""Write a Linux newc initramfs, including device nodes, without root."""

import gzip
from pathlib import Path
import stat
import sys


def write_entry(out, name: str, mode: int, data: bytes, inode: int,
                rdev_major: int = 0, rdev_minor: int = 0) -> None:
    encoded_name = name.encode() + b"\0"
    fields = (inode, mode, 0, 0, 2 if stat.S_ISDIR(mode) else 1,
              0, len(data), 0, 0, rdev_major, rdev_minor, len(encoded_name), 0)
    out.write(b"070701" + b"".join(f"{field:08x}".encode() for field in fields))
    out.write(encoded_name)
    out.write(b"\0" * (-(110 + len(encoded_name)) % 4))
    out.write(data)
    out.write(b"\0" * (-len(data) % 4))


def main() -> None:
    root, image = map(Path, sys.argv[1:])
    paths = sorted((path for path in root.rglob("*") if path.is_file() or path.is_dir()),
                   key=lambda path: path.relative_to(root).as_posix())
    with image.open("wb") as target:
        with gzip.GzipFile(filename="", mode="wb", compresslevel=9, mtime=0,
                           fileobj=target) as out:
            inode = 1
            for path in paths:
                name = path.relative_to(root).as_posix()
                data = path.read_bytes() if path.is_file() else b""
                write_entry(out, name, path.stat().st_mode, data, inode)
                inode += 1
            write_entry(out, "dev/console", stat.S_IFCHR | 0o600, b"", inode, 5, 1)
            write_entry(out, "dev/null", stat.S_IFCHR | 0o666, b"", inode + 1, 1, 3)
            write_entry(out, "TRAILER!!!", 0, b"", inode + 2)


if __name__ == "__main__":
    main()
