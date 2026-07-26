# scope_loopback

Continuous CAN FD bus output on a single board for oscilloscope or protocol analyser capture.

## What you'll learn

How to use `MODE_EXTERNAL_LB` to drive real differential signals on CANH/CANL through the
transceiver while the chip ACKs its own frames — no second node required. Useful for
measuring signal integrity, verifying transceiver wiring, or capturing known frames.

## Hardware

Single board. Connect your oscilloscope probes to CANH and CANL.

| Pin | GPIO |
|-----|------|
| SCK | 33 |
| MISO | 35 |
| MOSI | 32 |
| CS | 25 |

## What to expect

Frames transmit continuously at 10 ms intervals. Press any key in the Serial monitor to
step through data rates: 2 Mbps → 4 Mbps → 5 Mbps → back to 2 Mbps.

```
==========================
  CAN FD Scope Loopback
==========================
configure: OK  FSYS: 20000000 Hz
SID=0x123  DLC=8  interval=10 ms

Rate: 125 kbps nominal / 2 Mbps data  (press any key to cycle)
  125 kbps nominal / 2 Mbps data  frames=100
  125 kbps nominal / 2 Mbps data  frames=200

Rate: 125 kbps nominal / 4 Mbps data  (press any key to cycle)
```
