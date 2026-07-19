# Unit Tests

Native unit tests — no hardware required. Run with:

```bash
wsl -d Ubuntu -- bash -c "cd /mnt/c/Users/d1/repos/mcp2518fd/tests/unit && ~/.local/bin/pio test -e native"
```

## Coverage

| Area | Tests |
|---|---|
| `dlcToLen()` | DLC 0–8 (1:1), DLC 9–15 (FD jumps) |
| `calcBitTiming()` | All standard rates at 20 MHz and 40 MHz vs preset constants; TDC enabled/disabled/TDCO value; rejection of non-integer TQ, zero inputs, totalTQ < 3, rate too high; sample point 75–85%; SJW == TSEG2 |
| `calcTxTimeout()` | Minimum floor of 2 ms; reasonable range for real rates; slower rate → longer timeout |
| EID encode/decode | Roundtrip max/zero/known IDs; SID in bits[10:0]; EID in bits[28:11] |
| Filter OBJ/MASK | EXIDE bit set for EID OBJ; MIDE bit set for EID MASK; zero mask = don't-care; roundtrip known UDS ID |
| Register addresses | FIFO_CON/STA/UA for FIFO1 and FIFO2; 0x0C stride; FLTOBJ/FLTMSK 8-byte stride; FLTMSK offset +4 from FLTOBJ; FLTCON_REG groups of 4; FLTCON_BYTE wraps at 4; filter 31 address |
| FIFOCON/FIFOSTA bits | TXEN bit7, UINC bit8, TXREQ bit9, TFNRFNIF bit0, TXERR bit5, TXABT bit7 |
