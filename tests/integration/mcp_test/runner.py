"""Orchestrates upload + test for one suite or all three in sequence."""

from .upload import upload
from .suites import run_single_board, run_two_node
from .serial_io import safe_print

# Suite definitions — order matters for regression runs
SUITES = ["single_node", "id_filter", "two_node"]


def run_suite(env, port, port_b, baud, skip_upload):
    """Upload (unless skip_upload) and test one suite. Returns True on pass."""
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


def run_all(port, port_b, baud, skip_upload):
    """Run all three suites in sequence. Returns True only if all pass."""
    results = {}
    for env in SUITES:
        safe_print(f"\n{'='*50}")
        safe_print(f"SUITE: {env}")
        safe_print(f"{'='*50}")
        results[env] = run_suite(env, port, port_b, baud, skip_upload)

    safe_print("\n" + "="*50)
    safe_print("REGRESSION SUMMARY")
    safe_print("="*50)
    all_passed = True
    for env, passed in results.items():
        safe_print(f"  {env:<16}  {'PASS' if passed else 'FAIL'}")
        if not passed:
            all_passed = False
    safe_print("")
    safe_print("RESULT: " + ("PASS - all suites OK." if all_passed
                              else "FAIL - one or more suites failed."))
    return all_passed
