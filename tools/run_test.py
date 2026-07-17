"""
run_test.py — reset ESP32(s), trigger test, collect output, exit.

Usage:
  loopback (single board):
    python tools/run_test.py --env loopback --port COM4

  two_node (two boards):
    python tools/run_test.py --env two_node --port-a COM4 --port-b COM5

Options:
  --env       PlatformIO environment name (loopback | two_node)
  --port      Serial port for single-board tests
  --port-a    Serial port for node A (two-node tests)
  --port-b    Serial port for node B (two-node tests)
  --baud      Baud rate (default: 115200)
"""

import sys
import time
import argparse
import serial


def read_until_quiet(s, quiet_ms=500, max_s=5):
    """Read from serial until no new bytes arrive for quiet_ms, or max_s exceeded."""
    buf = b""
    deadline = time.time() + max_s
    last_rx  = time.time()
    while time.time() < deadline:
        n = s.in_waiting
        if n:
            buf += s.read(n)
            last_rx = time.time()
        elif (time.time() - last_rx) * 1000 > quiet_ms:
            break
        time.sleep(0.02)
    return buf


def reset_and_trigger(port, baud):
    """Open port, hardware-reset the board, wait for startup, send trigger, return output."""
    s = serial.Serial(port, baud, timeout=1)

    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)

    startup = read_until_quiet(s, quiet_ms=500, max_s=4)
    if startup:
        sys.stdout.buffer.write(startup)
        sys.stdout.buffer.flush()

    s.write(b"\n")

    response = read_until_quiet(s, quiet_ms=500, max_s=4)
    if response:
        sys.stdout.buffer.write(response)
        sys.stdout.buffer.flush()

    s.close()


def run_loopback(port, baud):
    reset_and_trigger(port, baud)


def run_two_node(port_a, port_b, baud):
    # TODO: open both ports, coordinate reset + trigger across two boards
    print("two_node test not yet implemented.")


def main():
    parser = argparse.ArgumentParser(description="MCP2518FD test runner")
    parser.add_argument("--env",    required=True, choices=["loopback", "two_node"])
    parser.add_argument("--port",   default="COM4")
    parser.add_argument("--port-a", default="COM4")
    parser.add_argument("--port-b", default="COM5")
    parser.add_argument("--baud",   type=int, default=115200)
    args = parser.parse_args()

    if args.env == "loopback":
        run_loopback(args.port, args.baud)
    elif args.env == "two_node":
        run_two_node(args.port_a, args.port_b, args.baud)


if __name__ == "__main__":
    main()
