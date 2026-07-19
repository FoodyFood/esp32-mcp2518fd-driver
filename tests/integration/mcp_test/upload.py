"""PlatformIO upload wrapper."""

import os
import subprocess
import sys

# Repo root is two levels above this file (tests/integration/mcp_test/)
REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def upload(env, port):
    """
    Build and upload a PlatformIO environment to a port.
    Streams pio output to stdout. Returns True on success.
    """
    example_dir = os.path.join(REPO_ROOT, "examples", env)
    cmd = ["pio", "run", "-e", env, "--target", "upload", "--upload-port", port]

    print(f"\n>>> Uploading {env} to {port}")
    result = subprocess.run(cmd, cwd=example_dir)
    if result.returncode != 0:
        print(f">>> UPLOAD FAILED: {env} -> {port}")
        return False
    return True
