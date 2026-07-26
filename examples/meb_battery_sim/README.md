# meb_battery_sim

Impersonate a VW MEB battery on a CAN FD bus.

Flash the `simulator` environment to one board and it will transmit the full MEB
keepalive frame schedule — the same frames a real MEB battery pack sends to its
inverter. Any device on the bus that speaks the MEB protocol (such as a
[Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator) node) will
see a healthy, contactors-closed battery.

The `monitor` environment turns a second board into an ACK node that prints every
frame it receives. Use it to verify the simulator is working before connecting a
real Battery-Emulator node.

## What you'll learn

- How to transmit a strict multi-interval frame schedule (10 / 100 / 200 / 500 ms)
- How to implement the VAG CRC algorithm and rolling counter required by MEB frames
- How to mix CAN FD and classic CAN 2.0B frames on the same bus (NMH_Hybrid_01 is
  a classic CAN frame; all others are CAN FD)
- How to use 29-bit extended IDs alongside 11-bit standard IDs
- How to read a received frame and decode a status field from it

## Hardware required

- 2 × ESP32 with MCP2518FD (e.g. the breakout described in
  [docs/hardware.md](https://github.com/FoodyFood/esp32-mcp2518fd-driver/blob/main/docs/hardware.md))
- CAN bus wiring between the two boards: CANH→CANH, CANL→CANL, GND→GND
- 120 Ω termination resistor at each end of the bus

## Wiring

| Signal | ESP32 GPIO |
|--------|-----------|
| SCK    | 33        |
| MISO   | 35        |
| MOSI   | 32        |
| CS     | 25        |

## Setup

1. Flash `simulator` to the first board (COM4):
   ```
   pio run -e simulator --target upload
   ```

2. Flash `monitor` to the second board (COM3):
   ```
   pio run -e monitor --target upload
   ```

3. Open a serial monitor on each board at 115200 baud.

## Expected output

**Simulator (COM4):**
```
[SIM] MEB battery simulator running
[SIM] SOC=65%  voltage=370V  charge=22kW  discharge=100kW
```
No TX FAIL messages means the monitor board is ACKing correctly.

**Monitor (COM3):**
```
[MON] MEB monitor running — waiting for frames
[MON] BMS_20            id=0x000000CF  dlc= 8  FD +BRS  8B 07 09 AC 3F C8 05 5C
[MON] BMS_20            id=0x000000CF  dlc= 8  FD +BRS  CA 08 09 AC 3F C8 05 5C
[MON] BMS_22            id=0x12DD54D1  dlc= 8  FD +BRS  00 80 29 0A 00 00 74 1D
[MON] BMS_21            id=0x12DD54D0  dlc= 8  FD +BRS  00 00 00 00 00 FA E0 06
[MON] BMS_04            id=0x000005A2  dlc= 8  FD +BRS  50 8A 00 4B 10 00 00 00
[MON] NMH_Hybrid_01     id=0x1B00007B  dlc= 8  CAN      00 10 00 00 00 00 00 00
[MON] BMS_07            id=0x000005CA  dlc= 8  FD +BRS  92 02 00 00 00 20 00 00
[MON] KN_Hybrid_01      id=0x17F0007B  dlc= 8  FD +BRS  20 00 00 00 00 00 00 00
```

Things to check:
- BMS_20 arrives ~10 times per 100 ms block — the 10 ms interval is correct
- BMS_20 byte 1 low nibble counts 0→1→2→...→15→0 — rolling counter is correct
- BMS_20 byte 0 changes with every counter value and repeats identically on the
  next cycle — VAG CRC is correct
- BMS_20 byte 2 = `0x09` → bits[2:0] = `001` = BMS_mode 1 (HV_ACTIVE)
- NMH_Hybrid_01 shows `CAN` not `FD` — classic CAN 2.0B frame on the FD bus

## Connecting a real Battery-Emulator node

Replace the monitor board with a
[Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator) node
configured for the VW MEB battery profile. The simulator transmits BMS_mode=1
(HV_ACTIVE) in BMS_20, which is the signal Battery-Emulator uses to confirm the
battery contactors are closed.

When the Battery-Emulator node is running, the simulator will also receive BMS_20
frames back from it and print the decoded mode:
```
[SIM] RX BMS_20 mode=1 (HV_ACTIVE)
```

## Adjusting the simulated battery state

Edit the four `#define` values at the top of `src/main.cpp` and reflash:

```cpp
#define SIM_SOC_PERCENT      65    // 0–100 %
#define SIM_VOLTAGE_V        370   // pack voltage in volts
#define SIM_MAX_CHARGE_KW    22    // max charge power in kW
#define SIM_MAX_DISCHARGE_KW 100   // max discharge power in kW
```

## Frame schedule

All frame IDs, payloads, CRC constants and timing intervals are sourced directly
from [Battery-Emulator](https://github.com/dalathegreat/Battery-Emulator)
`Software/src/battery/MEB-BATTERY.cpp` and `MEB-BATTERY.h`. Nothing is invented.

| Frame         | ID           | DLC | Type    | Interval |
|---------------|--------------|-----|---------|----------|
| BMS_20        | 0x0CF        | 8   | FD+BRS  | 10 ms    |
| BMS_22        | 0x12DD54D1   | 8   | FD+BRS  | 100 ms   |
| BMS_21        | 0x12DD54D0   | 8   | FD+BRS  | 100 ms   |
| BMS_04        | 0x5A2        | 8   | FD+BRS  | 100 ms   |
| NMH_Hybrid_01 | 0x1B00007B   | 8   | Classic | 200 ms   |
| BMS_07        | 0x5CA        | 8   | FD+BRS  | 500 ms   |
| KN_Hybrid_01  | 0x17F0007B   | 8   | FD+BRS  | 500 ms   |
