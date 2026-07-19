"""PlatformIO build and upload wrapper."""

import os
import subprocess
import logging

log = logging.getLogger("mcp_test")

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", ".."))


def _harness_dir(env):
    return os.path.join(REPO_ROOT, "tests", "integration", env)


def build(env):
    """
    Build a PlatformIO environment without uploading.
    Returns True on success.
    """
    harness_dir = _harness_dir(env)
    cmd = ["pio", "run", "-e", env]
    log.info(">>> Building %s", env)
    result = subprocess.run(cmd, cwd=harness_dir, capture_output=False)
    if result.returncode != 0:
        log.error(">>> BUILD FAILED: %s", env)
        return False
    return True


def upload(env, port):
    """
    Build and upload a PlatformIO environment to a port.
    Returns True on success.
    """
    harness_dir = _harness_dir(env)
    cmd = ["pio", "run", "-e", env, "--target", "upload", "--upload-port", port]
    log.info(">>> Uploading %s to %s", env, port)
    result = subprocess.run(cmd, cwd=harness_dir, capture_output=False)
    if result.returncode != 0:
        log.error(">>> UPLOAD FAILED: %s -> %s", env, port)
        return False
    return True
