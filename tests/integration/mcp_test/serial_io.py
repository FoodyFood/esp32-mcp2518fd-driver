"""Serial I/O primitives shared by all test suites."""

import time
import serial
import logging

log = logging.getLogger("mcp_test")


def reset_board(s):
    """Hardware-reset the ESP32 via RTS."""
    s.setDTR(False)
    s.setRTS(True)
    time.sleep(0.1)
    s.setRTS(False)


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


def open_and_reset(port, baud):
    """Open a serial port and hardware-reset the board. Returns the open Serial."""
    s = serial.Serial(port, baud, timeout=1)
    reset_board(s)
    return s
