#!/usr/bin/env python3
"""Apply SOJ v2 detection fix and startup timing fix to xilinx.cpp"""

with open('src/xilinx.cpp', 'r') as f:
    content = f.read()

# ===== CHANGE 1: Startup timing - increase TCK and keep TLR unconditionally =====
old_startup = '''		_jtag->shiftIR(get_ircode(_ircode_map, "JSTART"), NULL, _irlen, Jtag::UPDATE_IR);
		/*
		* 22: Move to the RTI state and clock the
		*     startup sequence by applying a minimum         X     0   2000
		*     of 2000 clock cycles to the TCK.
		*
		* Spartan-6 can additionally require ISC_DISABLE while in RTI for the
		* final handoff into user mode, but cutting the startup clocks down to
		* a tiny value leaves the USER scan chains inactive on some boards.
		* Keep the long startup clock run, then do an extra ISC_DISABLE pulse
		* for Spartan-6 as a follow-up.
		*/
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2000);
		if (_fpga_family == SPARTAN6_FAMILY) {
			_jtag->shiftIR(get_ircode(_ircode_map, "ISC_DISABLE"), NULL,
				_irlen, Jtag::UPDATE_IR);
			_jtag->set_state(Jtag::RUN_TEST_IDLE);
			_jtag->toggleClk(64);
		}
		/*
		* 23: Move to the TLR state. The device is
		* now functional.                                    X     1   3
		*/
		_jtag->go_test_logic_reset();
		/* Some xc7s50 does not detect correct connected flash w/o this shift*/
		_jtag->shiftIR(tx_buf, rx_buf, _irlen);
		uint8_t ir_c = rx_buf[0] & 0x03;
		uint8_t isc_done = ((rx_buf[0] >> 2) & 0x01);
		uint8_t isc_ena  = ((rx_buf[0] >> 3) & 0x01);
		uint8_t init     = ((rx_buf[0] >> 4) & 0x01);
		uint8_t done     = ((rx_buf[0] >> 5) & 0x01);
		printf("Shift IR %02x\\n", rx_buf[0]);
		printf("ir: %x isc_done %x isc_ena %x init %x done %x\\n", ir_c, isc_done, isc_ena,
			init, done);

		if (!done) {
			read_register("STAT");
			/*
			 * Startup may need more time. Poll DONE bit with retries.
			 * CRITICAL: do NOT use go_test_logic_reset() during polling —
			 * TLR aborts the startup sequence on 7-series FPGAs. Instead,
			 * poll by shifting IR directly from RTI (which is safe) and
			 * wait in RTI between polls.
			 */
			bool startup_ok = false;
			const int tck_waits[] = {5000, 10000, 20000, 50000};
			for (int retry = 0; retry < (int)sizeof(tck_waits)/sizeof(tck_waits[0]); retry++) {
				_jtag->set_state(Jtag::RUN_TEST_IDLE);
				_jtag->toggleClk(tck_waits[retry]);
				/* Shift IR directly from RTI — does NOT abort startup */
				_jtag->shiftIR(tx_buf, rx_buf, _irlen);
				done = ((rx_buf[0] >> 5) & 0x01);
				if (_verbose > 0) {
					printf("Startup poll %d (%d TCKs): done=%d (IR=0x%02x)\\n",
					       retry + 1, tck_waits[retry], done, rx_buf[0]);
				}
				if (done) {
					startup_ok = true;
					break;
				}
			}
			if (startup_ok) {
				/* Now safe to go to TLR after startup complete */
				_jtag->go_test_logic_reset();
			} else {
				printWarn("Startup did not complete after JSTART (DONE=0) — "
				             "SOJ bridge may not forward SPI data");
				/* Still go to TLR for the scan chain to work */
				_jtag->go_test_logic_reset();
			}
		}
	}
}'''

new_startup = '''		_jtag->shiftIR(get_ircode(_ircode_map, "JSTART"), NULL, _irlen, Jtag::UPDATE_IR);
		/*
		* 22: Move to the RTI state and clock the startup sequence.
		* Xilinx UG470: min 100ms at 10MHz TCK = ~600k TCK at 6MHz.
		*/
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(600000);
		if (_fpga_family == SPARTAN6_FAMILY) {
			_jtag->shiftIR(get_ircode(_ircode_map, "ISC_DISABLE"), NULL,
				_irlen, Jtag::UPDATE_IR);
			_jtag->set_state(Jtag::RUN_TEST_IDLE);
			_jtag->toggleClk(64);
		}
		/* 23: Move to the TLR state. The device is now functional.
		 * CRITICAL: TLR is required to activate the SOJ bridge fabric
		 * (MMCM + USER scan chain SPI forwarding). DONE may never reach 1
		 * for SOJ bridge bitstreams, but TLR must still happen. */
		_jtag->go_test_logic_reset();
		/* Some xc7s50 does not detect correct connected flash w/o this shift*/
		_jtag->shiftIR(tx_buf, rx_buf, _irlen);
		uint8_t ir_c = rx_buf[0] & 0x03;
		uint8_t isc_done = ((rx_buf[0] >> 2) & 0x01);
		uint8_t isc_ena  = ((rx_buf[0] >> 3) & 0x01);
		uint8_t init     = ((rx_buf[0] >> 4) & 0x01);
		uint8_t done     = ((rx_buf[0] >> 5) & 0x01);
		printf("Shift IR %02x\\n", rx_buf[0]);
		printf("ir: %x isc_done %x isc_ena %x init %x done %x\\n", ir_c, isc_done, isc_ena,
			init, done);

		if (!done) {
			read_register("STAT");
		}
	}
}'''

if old_startup in content:
    content = content.replace(old_startup, new_startup)
    print("CHANGE 1: Startup timing applied (2000 -> 600000 TCK, TLR unconditionally)")
else:
    print("CHANGE 1: FAILED - old text not found")

# ===== CHANGE 2: Fix false-positive SOJ v2 detection =====
old_v2_detect = '''	/*
	 * If the version probe returned all zeros, the bridge might be SOJ v2
	 * but we sent the version query in v1 format. Check the raw response
	 * for v2 packet characteristics:
	 *   - jrx[0] has bit0 set (v2 start bit) or matches a known v2 header
	 *   - the response looks like a v2 packet rather than plain text
	 * Also try the version query in v2 format to confirm.
	 */
	if (looks_like_invalid_bridge_reply(rx, 5)) {
		/* Check if raw response looks like a v2 packet header */
		uint8_t rev0 = McsParser::reverseByte(jrx[0]);
		if ((rev0 & 0x01) && (_verbose > 0)) {
			printf("SOJ version probe: raw byte 0 (0x%02x, rev=0x%02x) "
			       "looks like v2 packet header - bridge is likely SOJ v2\\n",
			       jrx[0], rev0);
		}

		/* Try version query in SOJ v2 format */
		if (_verbose > 0) {
			printf("Trying SOJ v2 version query...\\n");
		}
		uint8_t v2_pkt[7];
		uint8_t v2_jrx[7];
		uint32_t v2_real_len = 6;  // 1 cmd + 5 padding
		uint32_t v2_kPktLen = v2_real_len + 2;  // header + extra
		v2_pkt[0] = ((0x1f & v2_real_len) << 3) | ((0x03 & 0x01) << 1) | 1;
		v2_pkt[1] = McsParser::reverseByte(0x01);  // version query cmd
		memset(&v2_pkt[2], 0, 5);

		_jtag->go_test_logic_reset();
		_jtag->shiftIR(get_ircode(_ircode_map, "USER1"), NULL, _irlen,
			Jtag::UPDATE_IR);
		_jtag->shiftDR(v2_pkt, v2_jrx, (v2_kPktLen - 1) * 8 + 8);
		_jtag->go_test_logic_reset();
		_jtag->flush();

		if (_verbose > 0) {
			printf("SOJ v2 version query raw:");
			for (size_t i = 0; i < sizeof(v2_jrx); i++)
				printf(" %02x", v2_jrx[i]);
			printf("\\n");
		}

		/* Try to decode v2 response as version string */
		/* v2 response: header(2) + reversed data */
		uint8_t v2_idx = 2;  // skip v2 header for short packets
		bool v2_has_data = false;
		for (uint32_t i = 0; i < 5; i++) {
			rx[i] = McsParser::reverseByte(v2_jrx[i + v2_idx]);
			if (rx[i] != 0x00 && rx[i] != 0xff)
				v2_has_data = true;
		}
		rx[5] = '\\0';

		if (v2_has_data && (_verbose > 0)) {
			printf("SOJ v2 version decoded: '%s'\\n", rx);
			float v2_ver = atof((const char *)rx);
			if (v2_ver >= 2.0f)
				return v2_ver;
			/* Even if version string isn't parseable, having
			 * non-zero data in v2 format means the bridge is v2 */
			return 2.0f;
		}

		/* If the raw version probe byte 0 looked like v2 header,
		 * assume v2 even without a version string */
		if ((rev0 & 0x01)) {
			if (_verbose > 0)
				printf("Bridge appears to be SOJ v2 (raw header match)\\n");
			return 2.0f;
		}
	}'''

new_v2_detect = '''	/*
	 * If the v1 version probe returned all zeros, the bridge might be SOJ v2
	 * (v1 format query -> v2 packet response). Check for genuine v2 evidence.
	 *
	 * WARNING: Impact-generated bridge bitstreams for Artix-7/Kintex-7 use
	 * SOJ v1 (USER4 scan chain). Only assume v2 when the response contains
	 * multiple non-zero data bytes — a single echoed command byte is NOT
	 * evidence of v2 bridge operation.
	 */
	if (looks_like_invalid_bridge_reply(rx, 5)) {
		/* Check if raw response looks like a v2 packet header */
		uint8_t rev0 = McsParser::reverseByte(jrx[0]);
		if ((rev0 & 0x01) && (_verbose > 0)) {
			printf("SOJ version probe: raw byte 0 (0x%02x, rev=0x%02x) "
			       "has v2 start bit set\\n",
			       jrx[0], rev0);
		}

		/* Try version query in SOJ v2 format to check for genuine v2 bridge */
		if (_verbose > 0) {
			printf("Trying SOJ v2 version query...\\n");
		}
		uint8_t v2_pkt[7];
		uint8_t v2_jrx[7];
		uint32_t v2_real_len = 6;  // 1 cmd + 5 padding
		uint32_t v2_kPktLen = v2_real_len + 2;  // header + extra
		v2_pkt[0] = ((0x1f & v2_real_len) << 3) | ((0x03 & 0x01) << 1) | 1;
		v2_pkt[1] = McsParser::reverseByte(0x01);  // version query cmd
		memset(&v2_pkt[2], 0, 5);

		_jtag->go_test_logic_reset();
		_jtag->shiftIR(get_ircode(_ircode_map, "USER1"), NULL, _irlen,
			Jtag::UPDATE_IR);
		_jtag->shiftDR(v2_pkt, v2_jrx, (v2_kPktLen - 1) * 8 + 8);
		_jtag->go_test_logic_reset();
		_jtag->flush();

		if (_verbose > 0) {
			printf("SOJ v2 version query raw:");
			for (size_t i = 0; i < sizeof(v2_jrx); i++)
				printf(" %02x", v2_jrx[i]);
			printf("\\n");
		}

		/* Decode v2 response: header(2) + reversed data */
		uint8_t v2_idx = 2;  // skip v2 header for short packets
		int v2_nonzero_noncmd = 0;  // exclude echoed cmd byte at offset 1
		for (uint32_t i = 0; i < 5; i++) {
			uint8_t b = McsParser::reverseByte(v2_jrx[i + v2_idx]);
			if (b != 0x00 && b != 0xff && i != 1)  // byte 1 = echoed cmd
				v2_nonzero_noncmd++;
		}

		/* Only trust v2 if there are 2+ non-zero data bytes beyond the
		 * echoed command — a single byte is just the query echo */
		if (v2_nonzero_noncmd >= 2 && (_verbose > 0)) {
			for (uint32_t i = 0; i < 5; i++)
				printf(" %02x", McsParser::reverseByte(v2_jrx[i + v2_idx]));
			printf("\\nSOJ v2 version string has %d data bytes — assuming v2\\n",
			       v2_nonzero_noncmd);
			return 2.0f;
		}

		if ((rev0 & 0x01) && (_verbose > 0))
			printf("Raw v2 header bit set, but no version string data — staying v1\\n");
	}'''

if old_v2_detect in content:
    content = content.replace(old_v2_detect, new_v2_detect)
    print("CHANGE 2: SOJ v2 detection fix applied")
else:
    print("CHANGE 2: FAILED - old text not found")

with open('src/xilinx.cpp', 'w') as f:
    f.write(content)

print("File written successfully")