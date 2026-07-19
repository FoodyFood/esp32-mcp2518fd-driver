"""Orchestrates upload + test for one suite or all three in sequence."""

import logging
from .upload import build, upload
from .suites import run_single_board, run_two_node

log = logging.getLogger("mcp_test")

# Suite definitions — order matters for regression runs
SUITES = ["single_node", "id_filter", "two_node"]


def run_suite(env, port, port_b, baud, skip_upload, build_only=False):
    """Build (and optionally upload + test) one suite. Returns True on pass."""
    if build_only:
        return build(env)

    if env == "two_node":
        if not skip_upload:
            if not upload("two_node", port):
                return False
            if not upload("two_node", port_b):
                return False
        return run_two_node(port, port_b, baud)
    else:
        if not skip_upload:
            if not upload(env, port):
                return False
        passed, _ = run_single_board(port, baud, label=env)
        return passed


def run_all(port, port_b, baud, skip_upload, build_only=False):
    """Run all suites in sequence. Returns True only if all pass."""
    results = {}
    for env in SUITES:
        log.info("")
        log.info("=" * 50)
        log.info("SUITE: %s", env)
        log.info("=" * 50)
        results[env] = run_suite(env, port, port_b, baud, skip_upload, build_only)

    log.info("")
    log.info("=" * 50)
    log.info("REGRESSION SUMMARY")
    log.info("=" * 50)
    all_passed = True
    for env, passed in results.items():
        status = "PASS" if passed else "FAIL"
        (log.info if passed else log.error)("  %-16s  %s", env, status)
        if not passed:
            all_passed = False
    log.info("")
    if all_passed:
        log.info("RESULT: PASS - all suites OK.")
    else:
        log.error("RESULT: FAIL - one or more suites failed.")
    return all_passed
