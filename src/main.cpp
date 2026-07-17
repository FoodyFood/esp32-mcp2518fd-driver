void runTest()
{
    Serial.println();
    Serial.println("==========================");

    can.reset();
    delay(20);

    Serial.println("After reset:");
    dumpCiCON();

    // Stay in configuration mode
    can.setMode(MODE_CONFIG);

    // Nominal bit timing
    // BRP   = 0  (÷1)
    // TSEG1 = 30 (31 TQ)
    // TSEG2 = 7  (8 TQ)
    // SJW   = 7  (8 TQ)
    uint32_t nbtcfg =
          (0u  << 24)
        | (30u << 16)
        | (7u  << 8)
        | (7u);

    // Data bit timing
    uint32_t dbtcfg =
          (0u << 24)
        | (14u << 16)
        | (3u << 8)
        | (3u);

    can.write32(REG_CiNBTCFG, nbtcfg);
    can.write32(REG_CiDBTCFG, dbtcfg);

    Serial.printf("CiNBTCFG = %08lX\n", can.read32(REG_CiNBTCFG));
    Serial.printf("CiDBTCFG = %08lX\n", can.read32(REG_CiDBTCFG));

    can.setMode(MODE_INTERNAL_LB);

    Serial.println();
    Serial.println("After LOOPBACK:");
    dumpCiCON();
}