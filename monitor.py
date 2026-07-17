"""
monitor.py - reset ESP32, collect startup output, send one trigger, collect response, exit.
Usage: python monitor.py [port] [baud] [trigger_char]
Defaults: COM4 115200 newline
"""
import sys
import time
import serial

PORT    = sys.argv[1] if len(sys.argv) > 1 else "COM4"
BAUD    = int(sys.argv[2]) if len(sys.argv) > 2 else 115200
TRIGGER = sys.argv[3].encode() if len(sys.argv) > 3 else b"\n"

def read_until_quiet(s, quiet_ms=500, max_s=5):
    """Read until no new bytes arrive for quiet_ms, or max_s exceeded."""
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

s = serial.Serial(PORT, BAUD, timeout=1)

# Hardware reset via RTS
s.setDTR(False)
s.setRTS(True)
time.sleep(0.1)
s.setRTS(False)

startup = read_until_quiet(s, quiet_ms=500, max_s=4)
if startup:
    sys.stdout.buffer.write(startup)
    sys.stdout.buffer.flush()

# Send trigger
s.write(TRIGGER)

response = read_until_quiet(s, quiet_ms=500, max_s=4)
if response:
    sys.stdout.buffer.write(response)
    sys.stdout.buffer.flush()

s.close()
