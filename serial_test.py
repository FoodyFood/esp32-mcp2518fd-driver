import serial
import time

out = open(r"C:\Users\d1\repos\mcp2518fd\serial_out.txt", "w")

try:
    s = serial.Serial("COM4", 115200, timeout=1)
    out.write("Port opened OK\n")

    # Reset ESP32
    s.dtr = False
    s.rts = True
    time.sleep(0.2)
    s.rts = False
    out.write("Reset sent\n")
    time.sleep(3)

    # Read startup
    buf = b""
    deadline = time.time() + 3
    while time.time() < deadline:
        if s.in_waiting:
            buf += s.read(s.in_waiting)
        time.sleep(0.05)
    out.write("STARTUP:\n" + buf.decode("utf-8", errors="replace") + "\n")

    # Trigger test
    s.write(b"\n")
    out.write("Keypress sent\n")

    buf = b""
    deadline = time.time() + 3
    while time.time() < deadline:
        if s.in_waiting:
            buf += s.read(s.in_waiting)
        time.sleep(0.05)
    out.write("RESPONSE:\n" + buf.decode("utf-8", errors="replace") + "\n")

    s.close()

except Exception as e:
    out.write("ERROR: " + str(e) + "\n")

out.close()
