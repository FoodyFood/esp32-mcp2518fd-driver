"""
run_test.py — reset ESP32(s), trigger test, collect output, exit non-zero on FAIL.

Usage:
  single_node (single board):
    python tools/run_test.py --env single_node --port COM4

  id_filter (single board):
    python tools/run_test.py --env id_filter --port COM4

  two_node (two boards):
    python tools/run_test.py --env two_node --port-a COM4 --port-b COM3

Options:
  --env       Environment to run (single_node | id_filter | two_node)
  --port      Serial port for single-board tests
  --port-a    Serial port for node A
  --port-b    Serial port for node B
  --baud      Baud rate (default: 115200)
"""

import sys
import time
import argparse
import threading
import serial


def read_until_quiet(s, quiet_ms=500, max_s=10):
    """Read from serial until no new bytes arrive for quiet_ms, or max_s exceeded."""
    buf = b""
    deadline = time.time() + max_s
    last_rx = time.time()
    while time.time() < deadline:
        n = s.in_waiting
        if n:
            buf += s.read(n)
            last_rx = time.time()
        elif (time.time() - last_rx) * 1000 > quiet_ms:
            break
        time.sleep(0.02)
    return buf


def reset_board(s):
    """Hardware-reset the ESP32 via RTS."""
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)


def run_loopback(port, baud):
    s = serial.Serial(port, baud, timeout=1)
    reset_board(s)

    startup = read_until_quiet(s, quiet_ms=500, max_s=4)
    if startup:
        sys.stdout.buffer.write(startup)
        sys.stdout.buffer.flush()

    s.write(b"\n")

    response = read_until_quiet(s, quiet_ms=500, max_s=6)
    if response:
        sys.stdout.buffer.write(response)
        sys.stdout.buffer.flush()

    s.close()
    return b"FAIL" not in response


def _node_worker(port, baud, trigger, label, results, output_lock, start_delay=0):
    """
    Worker thread for one node.
    Resets board, waits for startup, sends trigger byte, collects output.
    start_delay: seconds to wait before sending the trigger (lets the other node get ready first).
    """
    s = serial.Serial(port, baud, timeout=1)
    reset_board(s)

    startup = read_until_quiet(s, quiet_ms=500, max_s=4)

    with output_lock:
        for line in startup.decode(errors="replace").splitlines():
            _print(f"[{label}] {line}")

    if start_delay:
        time.sleep(start_delay)

    s.write(trigger.encode() + b"\n")

    response = read_until_quiet(s, quiet_ms=800, max_s=30)
    s.close()

    lines = response.decode(errors="replace").splitlines()
    results[label] = lines

    with output_lock:
        for line in lines:
            _print(f"[{label}] {line}")


def _print(s):
    """Print safely on Windows consoles that don't support all Unicode."""
    sys.stdout.buffer.write((s + "\n").encode("utf-8", errors="replace"))
    sys.stdout.buffer.flush()


def run_two_node(port_a, port_b, baud):
    results = {}
    output_lock = threading.Lock()

    # Reset both boards simultaneously, no delays, no coordination.
    # Each node operates independently — the firmware handles timing.
    ta = threading.Thread(
        target=_node_worker,
        args=(port_a, baud, "A", "A", results, output_lock),
        kwargs={"start_delay": 0},
        daemon=True)
    tb = threading.Thread(
        target=_node_worker,
        args=(port_b, baud, "B", "B", results, output_lock),
        kwargs={"start_delay": 0},
        daemon=True)

    # Reset and trigger both boards as close together as possible
    ta.start()
    tb.start()
    ta.join()
    tb.join()

    # Scan both outputs for failures
    failed = False
    for label, lines in results.items():
        for line in lines:
            if "FAIL" in line:
                failed = True
                print(f"[{label}] ^^^ FAILURE DETECTED")

    _print("")
    if failed:
        _print("RESULT: FAIL - one or more assertions failed.")
    else:
        a_ok = any("NODE A" in l for l in results.get("A", []))
        b_ok = any("NODE B" in l for l in results.get("B", []))
        if a_ok and b_ok:
            _print("RESULT: PASS - all assertions OK on both nodes.")
        else:
            _print("RESULT: INCOMPLETE - one or both nodes did not produce output.")
            failed = True

    return not failed


def main():
    parser = argparse.ArgumentParser(description="MCP2518FD test runner")
    parser.add_argument("--env",    required=True, choices=["single_node", "id_filter", "two_node"])
    parser.add_argument("--port",   default="COM4")
    parser.add_argument("--port-a", default="COM4")
    parser.add_argument("--port-b", default="COM3")
    parser.add_argument("--baud",   type=int, default=115200)
    args = parser.parse_args()

    if args.env in ("single_node", "id_filter"):
        ok = run_loopback(args.port, args.baud)
    else:
        ok = run_two_node(args.port_a, args.port_b, args.baud)

    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
