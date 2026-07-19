# SPEC-007 — CAN FD Battery Simulator Example (Kia 64 FD + VW MEB)

## Status
Pending

## Dependencies
- SPEC-001 (29-bit extended ID) — required for VW MEB
- SPEC-002 (acceptance filters) — required to filter response frames by SID
- SPEC-003 (bus error detection + TX error detail) — required for robust TX feedback

## Use case
UC-8 — CAN FD Battery Simulator

## Goal
Create `examples/battery_simulator/` — a self-contained PlatformIO example that impersonates
a real EV battery on a CAN FD bus. A second node (running `two_node` or `bus_monitor`) or a
real inverter/charger can be connected and will see valid battery frames.

Two battery profiles are implemented, selected at compile time via a build flag:

- **`-DBATTERY=KIA_64FD`** — Kia 64 kWh FD battery. 11-bit SIDs. 63-frame startup sequence
  then periodic PID polling. No CRC. Source: `Battery-Emulator/KIA-64FD-BATTERY.cpp`.

- **`-DBATTERY=VW_MEB`** — VW MEB battery. 29-bit extended IDs. VAG 0x2F CRC on selected
  frames. Multi-interval TX schedule (10 / 20 / 40 / 50 / 100 / 200 / 500 ms / 1 s).
  Source: `Battery-Emulator/MEB-BATTERY.cpp`.

## Bus parameters
- Nominal: 500 kbps
- Data: 2 Mbps
- Mode: MODE_NORMAL

## Acceptance criteria

### AC-1 — Kia 64 FD profile
- On boot, transmits the 63-frame startup sequence with correct SIDs and payloads
  (verified against `KIA-64FD-BATTERY.h` message_1 through message_63)
- After startup, transmits PID poll frame `0x7E4` every 200 ms
- Receives PID response on `0x7EC` and prints decoded SOC/voltage to Serial
- Serial monitor shows `[KIA64FD] TX startup frame N/63` during startup
- Serial monitor shows `[KIA64FD] TX poll` and `[KIA64FD] RX 0x7EC` during normal operation

### AC-2 — VW MEB profile
- Transmits all required keepalive frames on correct intervals:
  - 10 ms: `ESC_51_Auth` (0xFC) with rolling counter and VAG CRC
  - 20 ms: `ESP_21` (0xFD) with rolling counter and VAG CRC
  - 40 ms: `Airbag_01` (0x40) with rolling counter and VAG CRC
  - 50 ms: `EM1_01` (0xC0) with rolling counter and VAG CRC
  - 100 ms: `HVK_01` (0x503), `Klemmen_Status_01` (0x3C0), `Motor_14` (0x3BE),
            `Motor_54` (0x14C), `HVLM_14` (0x272) with rolling counters and VAG CRC
  - 200 ms: `MSG_HYB_30` (0x153), `NMH_*` frames
  - 500 ms: `eTM_01`, `HVEM_04`, `Klima_EV_06`, `ORU_01`, `Standklima_01`
  - 1 s: `Diagnose_01` (0x6B2), `Motor_Code_01` (0x641), `Reichweite_01`, `Systeminfo_01`,
         `Temperaturen_01`
- VAG CRC is correct on all frames that require it (verified by a second node reading the frames)
- Receives `BMS_20` (0xCF) from a real or simulated battery and prints BMS mode to Serial
- Serial monitor shows `[MEB] TX <frame_name> <interval>ms` for each scheduled frame
- Serial monitor shows `[MEB] RX BMS_20 mode=N` when BMS_20 is received

### AC-3 — Both profiles
- `configure(500000, 2000000, MODE_NORMAL)` returns `CanStatus::OK` on boot
- TX failures are reported: `[BATT] TX FAIL <frame_name>` printed to Serial
- Bus errors are reported using the `getErrors()` API from SPEC-003
- Example builds cleanly for both `kia_64fd` and `vw_meb` PlatformIO environments
- No regressions in `loopback` or `two_node` after this example is added

## platformio.ini environments
```
[env:kia_64fd]
build_flags = -DBATTERY=KIA_64FD
upload_port = COM4

[env:vw_meb]
build_flags = -DBATTERY=VW_MEB
upload_port = COM4
```

## Source reference
All frame IDs, payloads, CRC constants and timing intervals are taken directly from:
- `Battery-Emulator/Software/src/battery/KIA-64FD-BATTERY.h`
- `Battery-Emulator/Software/src/battery/MEB-BATTERY.cpp`
- `Battery-Emulator/Software/src/battery/MEB-BATTERY.h`

Do not invent frame content. Every byte must be traceable to the Battery-Emulator source.

## Notes
- This example does not implement ISO-TP or UDS — it only emits the periodic keepalive frames
  that cause a real device to see a healthy battery. ISO-TP is a future extension.
- The VAG CRC implementation (`vw_crc_calc`) is ported directly from `MEB-BATTERY.cpp`.
  Verify the output against known-good values from the Battery-Emulator source before use.
- 29-bit IDs require SPEC-001 to be Done. Do not start MEB implementation until SPEC-001
  is verified on hardware.
