"""Test suite implementations for each example."""

import logging
import threading
from .serial_io import open_and_reset, read_until_quiet

log = logging.getLogger("mcp_test")


def run_single_board(port, baud, label="TEST"):
    """
    Run a single-board test (single_node or id_filter).
    Returns (passed: bool, output: str).
    """
    s = open_and_reset(port, baud)

    startup = read_until_quiet(s, quiet_ms=500, max_s=4)
    output = startup.decode(errors="replace")

    s.write(b"\n")
    response = read_until_quiet(s, quiet_ms=500, max_s=10)
    s.close()

    output += response.decode(errors="replace")
    for line in output.splitlines():
        log.info("[%s] %s", label, line)

    passed = b"FAIL" not in response
    return passed, output


def _node_worker(port, baud, trigger, label, results, lock):
    """Worker thread for one node of a two-node test."""
    s = open_and_reset(port, baud)

    startup = read_until_quiet(s, quiet_ms=500, max_s=4)
    with lock:
        for line in startup.decode(errors="replace").splitlines():
            log.info("[%s] %s", label, line)

    s.write(trigger.encode() + b"\n")
    response = read_until_quiet(s, quiet_ms=800, max_s=30)
    s.close()

    lines = response.decode(errors="replace").splitlines()
    results[label] = lines
    with lock:
        for line in lines:
            log.info("[%s] %s", label, line)


def run_two_node(port_a, port_b, baud):
    """Run the two-node test. Returns (passed: bool)."""
    results = {}
    lock = threading.Lock()

    ta = threading.Thread(target=_node_worker,
                          args=(port_a, baud, "A", "A", results, lock), daemon=True)
    tb = threading.Thread(target=_node_worker,
                          args=(port_b, baud, "B", "B", results, lock), daemon=True)
    ta.start()
    tb.start()
    ta.join()
    tb.join()

    failed = any(
        "FAIL" in line
        for lines in results.values()
        for line in lines
    )

    if failed:
        log.error("RESULT: FAIL - one or more assertions failed.")
        return False

    a_ok = any("NODE A" in l for l in results.get("A", []))
    b_ok = any("NODE B" in l for l in results.get("B", []))
    if a_ok and b_ok:
        log.info("RESULT: PASS - all assertions OK on both nodes.")
        return True

    log.error("RESULT: INCOMPLETE - one or both nodes did not produce output.")
    return False
