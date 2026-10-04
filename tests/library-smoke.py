#!/usr/bin/env python3
"""Exercise persistent local game entries and safe direct launching."""

import os
from pathlib import Path
import subprocess
import tempfile


binary = Path(__file__).resolve().parents[1] / "build/qauntum-library"
with tempfile.TemporaryDirectory() as directory:
    environment = {**os.environ, "QAUNTUM_LIBRARY_DIR": directory}

    def run(*args, expected=0):
        result = subprocess.run(
            [str(binary), "player1", *args], env=environment,
            capture_output=True, text=True, check=False,
        )
        assert result.returncode == expected, (args, result.stderr)
        return result.stdout

    run("add", "Test Game", "/usr/bin/true")
    assert "Test Game" in run("list")
    assert "Test Game" in run("search", "test")
    assert "Test Game" not in run("search", "missing")
    run("favorite", "Test Game")
    assert run("list").startswith("*\tTest Game")
    run("launch", "Test Game", expected=1 if os.geteuid() == 0 else 0)
    run("add", "Test Game", "/usr/bin/true", expected=1)
    run("add", "Bad", "/not/an/executable", expected=1)
    assert "Test Game" in run("list")
    print("Local game library smoke test passed")
