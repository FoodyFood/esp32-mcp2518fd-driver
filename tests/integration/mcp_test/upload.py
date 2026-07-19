"""PlatformIO upload wrapper."""

import os
import subprocess
import logging

log = logging.getLogger("mcp_test")

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def upload(env, port):
    """
    Build and upload a PlatformIO environment to a port.
    Returns True on success.
    """
    example_dir = os.path.join(REPO_ROOT, "examples", env)
    cmd = ["pio", "run", "-e", env, "--target", "upload", "--upload-port", port]

    log.info(">>> Uploading %s to %s", env, port)
    result = subprocess.run(cmd, cwd=example_dir,
                            capture_output=False)
    if result.returncode != 0:
        log.error(">>> UPLOAD FAILED: %s -> %s", env, port)
        return False
    return True
