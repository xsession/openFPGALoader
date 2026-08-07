// SPDX-License-Identifier: Apache-2.0
/*
 * SOJ v2 SPI RDID diagnostic tool
 *
 * Sends RDID (0x9F) through the SOJ v2 bridge and traces:
 *   - TX packet bytes (raw and bit-reversed)
 *   - RX response bytes (raw and bit-reversed)
 *   - Data extracted at every possible byte offset
 *   - Which offset yields a valid JEDEC manufacturer ID
 *
 * Usage: ./openFPGALoader --spi-debug -c digilent_hs3 --fpga-part xc7a200tffg1156
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>
#include <string>

#include "utils/common.hpp"
#include "utils/display.hpp"
#include "utils/part.hpp"
#include "parsers/mcsParser.hpp"
#include "protocols/jtag.hpp"

static bool looks_like_valid_manufacturer(uint8_t mfr)
{
    static const uint8_t known[] = {
        0xEF, 0xC2, 0x20, 0x01, 0x1C, 0xBF,
        0x9D, 0xBA, 0x1F, 0xA1, 0x37, 0x64, 0x1D
    };
    for (auto id : known)
        if (mfr == id)
            return true;
    return false;
}

int run_spi_debug(Jtag *jtag, const std::string &fpga_part)
{
    printf("\n=== SOJ v2 SPI RDID Diagnostic ===\n\n");

    uint32_t idcode = jtag->get_target_device_id();
    printf("Target IDCODE: 0x%08x\n", idcode);

    Part part;
    if (!part.open(fpga_part)) {
        printError("Unknown part: " + fpga_part);
        return 1;
    }
    int irlen = part.getIRLength();
    printf("IR length: %d\n", irlen);

    /*
     * Xilinx SOJ v2 packet format (short, mode=0x01):
     * TX: [byte0: len<<3|mode<<1|1] [byte1: cmd_reversed] [byte2..: data_reversed...]
     * RX: [byte0: status1] [byte1: status2] [byte2: bridge_status] [byte3..: data_reversed...]
     *
     * For RDID (0x9F), len=4 data bytes, real_len=5 (cmd+data), kPktLen=7
     *
     * BUG: spi_put_v2() uses idx=2 for short packets, but the bridge
     *      returns 3 header bytes, so data starts at idx=3.
     */

    uint8_t cmd = 0x9F;
    uint32_t len = 4;
    const uint32_t real_len = len + 1;
    uint32_t kPktLen = real_len + 2;
    uint8_t mode = 0x01;

    uint8_t pkt[kPktLen];
    uint8_t jrx[kPktLen];
    uint32_t idx = 0;

    pkt[idx++] = ((0x1f & real_len) << 3) | ((0x03 & mode) << 1) | 1;
    pkt[idx++] = McsParser::reverseByte(cmd);
    memset(&pkt[idx], 0, len);
    idx += len;

    const uint32_t xfer_bit_len = (kPktLen - 1) * 8 + 8;

    printf("Packet params: real_len=%lu, mode=%d, kPktLen=%lu, xfer_bit_len=%lu\n\n",
           (unsigned long)real_len, mode, (unsigned long)kPktLen, (unsigned long)xfer_bit_len);

    printf("TX packet bytes (sent to JTAG):\n");
    for (uint32_t i = 0; i < kPktLen; i++)
        printf("  [%lu] 0x%02x (bit-rev: 0x%02x)\n",
               (unsigned long)i, pkt[i], McsParser::reverseByte(pkt[i]));

    uint8_t user1_ir = 0x1E;
    jtag->shiftIR(&user1_ir, NULL, irlen);
    jtag->shiftDR(pkt, jrx, xfer_bit_len);
    jtag->go_test_logic_reset();
    jtag->flush();

    printf("\nRX response bytes (from JTAG):\n");
    for (uint32_t i = 0; i < kPktLen; i++)
        printf("  [%lu] 0x%02x (bit-rev: 0x%02x)\n",
               (unsigned long)i, jrx[i], McsParser::reverseByte(jrx[i]));

    printf("\n=== Data extraction at each possible offset ===\n\n");
    for (int off = 0; off <= (int)kPktLen - (int)len; off++) {
        uint8_t extracted[4];
        for (uint32_t i = 0; i < len; i++)
            extracted[i] = McsParser::reverseByte(jrx[i + off]);

        uint32_t jedec = (extracted[0] << 16) | (extracted[1] << 8) | extracted[2];
        uint8_t mfr = (jedec >> 16) & 0xFF;

        printf("offset=%d: ", off);
        for (uint32_t i = 0; i < len; i++)
            printf("%02x ", extracted[i]);
        printf("-> JEDEC 0x%06x (mfr=0x%02x)", jedec, mfr);
        if (looks_like_valid_manufacturer(mfr))
            printf("  *** VALID MANUFACTURER ***\n");
        else
            printf("\n");
    }

    printf("\n=== Current code path (idx=2) ===\n");
    uint8_t rx_cur[4];
    for (uint32_t i = 0; i < len; i++)
        rx_cur[i] = McsParser::reverseByte(jrx[i + 2]);
    uint32_t jedec_cur = (rx_cur[0] << 16) | (rx_cur[1] << 8) | rx_cur[2];
    printf("Extracted: %02x %02x %02x %02x -> JEDEC 0x%06x\n",
           rx_cur[0], rx_cur[1], rx_cur[2], rx_cur[3], jedec_cur);

    printf("\n=== Corrected extraction (idx=3) ===\n");
    uint8_t rx_fix[4];
    for (uint32_t i = 0; i < len; i++)
        rx_fix[i] = McsParser::reverseByte(jrx[i + 3]);
    uint32_t jedec_fix = (rx_fix[0] << 16) | (rx_fix[1] << 8) | rx_fix[2];
    printf("Extracted: %02x %02x %02x %02x -> JEDEC 0x%06x\n",
           rx_fix[0], rx_fix[1], rx_fix[2], rx_fix[3], jedec_fix);

    printf("\n=== Diagnostic complete ===\n");
    return 0;
}
