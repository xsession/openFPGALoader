// Lattice MachXO3D specific programming
#include "vendors/lattice.hpp"
#include "parsers/rawParser.hpp"
#include <cstring>


bool Lattice::programFeatureRow_MachXO3D(uint8_t* feature_row)
{
	uint8_t tx[16] = { 0 };
	uint8_t rx[15] = { 0 };

	for (int i = 0; i < 12; i++)
		tx[i] = feature_row[i];

	if (_verbose) {
		printf("\tProgramming feature row: [0x");
		for (int i = 11; i >= 0; i--) {
			printf("%02x", feature_row[i]);
		}
		printf("]\n");
	}

	wr_rd(PROG_FEATURE_ROW, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	if (_verbose || _verify) {
		wr_rd(READ_FEATURE_ROW, NULL, 0, rx, 15);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	}

	if (_verbose) {
		printf("\tReadback Feature Row: [0x");
		for(int i = 11; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	if (_verify) {
		for(int i = 0; i < 15; i++) {
			if (feature_row[i] != rx[i]) {
				printf("\tVerify Failed...\n");
				return false;
			}
		}
	}

	return true;
}

bool Lattice::programFeabits_MachXO3D(uint32_t feabits)
{
	uint8_t tx[4] = { 0 };
	uint8_t rx[5] = { 0 };

	memset(tx, 0, sizeof(tx));
	for (int i = 0; i < 4; i++) {
		tx[i] = (feabits >> (8*i)) & 0xff;
	}

	if (_verbose) {
		printf("\tProgramming FEAbits: [0x");
		for (int i = 3; i >= 0; i--) {
			printf("%02x", tx[i]);
		}
		printf("]\n");
	}

	wr_rd(PROG_FEABITS, tx, 4, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	if (_verbose || _verify) {
		wr_rd(READ_FEABITS, NULL, 0, rx, 5);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	}

	if (_verbose) {
		printf("\tReadback Feabits: [0x");
		for(int i = 4; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	if (_verify) {
		for(int i = 0; i < 4; i++) {
			if (((feabits >> (8*i)) & 0xff) != rx[i]) {
				printf("\tVerify Failed...\n");
				return false;
			}
		}
	}

	return true;
}

bool Lattice::programPubKey_MachXO3D(uint8_t* pubkey)
{
	uint8_t rxkey[PUBKEY_LENGTH_BYTES] = { 0 };
	uint8_t tx[16];
	int i;

	if (_verbose) {
		printf("\tProgramming ECDSA PubKey: [");
		for (i = 0; i < PUBKEY_LENGTH_BYTES; i++) {
			printf("%02x", pubkey[i]);
		}
		printf("]\n");
	}

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[63 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY0, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[47 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY1, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[31 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY2, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	for(i = 0; i < 16; i++) {
		tx[i] = pubkey[15 - i];
	}
	wr_rd(PROG_ECDSA_PUBKEY3, tx, 16, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(0xff, NULL, 0, NULL, 0);
	if (!pollBusyFlag())
		return false;

	if (_verbose || _verify) {
		/* read the current feature row */
		wr_rd(READ_ECDSA_PUBKEY0, NULL, 0, rxkey, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		wr_rd(READ_ECDSA_PUBKEY1, NULL, 0, rxkey + 16, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		wr_rd(READ_ECDSA_PUBKEY2, NULL, 0, rxkey + 32, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		wr_rd(READ_ECDSA_PUBKEY3, NULL, 0, rxkey + 48, 16);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	}

	if (_verbose) {
		printf("Readback PubKey: [");
		for (i=PUBKEY_LENGTH_BYTES-1; i >= 0; i--) {
			printf("%02x", rxkey[i]);
			if (i && (i%16 == 0)) printf(" ");
		}
		printf("]\n");
	}

	if (_verify) {
		for (int i = 0; i < PUBKEY_LENGTH_BYTES; i++) {
			if (pubkey[i] != rxkey[PUBKEY_LENGTH_BYTES - i - 1]) {
				printf("\tVerify Failed...\n");
				return false;
			}
		}
	}

	return true;
}

bool Lattice::program_fea_MachXO3D()
{
	bool err;
	uint8_t rx[15] = { 0 };
	uint8_t tx[16] = { 0 };
	bool same = true;

	FeaParser _fea(_filename, _verbose);
	printInfo("Open file: ", false);
	printSuccess("DONE");

	err = _fea.parse();
	printInfo("Parse file: ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}
	if (_verbose)
		_fea.displayHeader();

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 with operand of 0x08 (Enable Offline mode) */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* read the current feature row */
	wr_rd(READ_FEATURE_ROW, NULL, 0, rx, 12);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	if (_verbose) {
		printf("Read Feature Row: [0x");
		for(int i = 11; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	uint8_t* feature_row = (uint8_t*)_fea.featuresRow();
	for (int i = 0; i < 12; i++) {
		if (feature_row[i] != rx[i])
			same = false;
	}

	/* read the current FEAbits */
	uint32_t feabits = _fea.feabits();
	wr_rd(READ_FEABITS, NULL, 0, rx, 6);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	if (_verbose) {
		printf("Read Feabits: [0x");
		for(int i = 4; i >= 0; i--) {
			printf("%02x", rx[i]);
		}
		printf("]\n");
	}

	for (int i = 0; i < 4; i++) {
		if ((feabits >> (i * 8) & 0xff) != rx[i])
			same = false;
	}

	printf("Feature Row / Feabits Compare: %s\n", same ? "Same" : "Different");
	if (same == false) {
		/* LSC_INIT_ADDRESS */
		tx[0] = (uint8_t)((FLASH_SEC_FEA >> 8) & 0xff);
		tx[1] = (uint8_t)((FLASH_SEC_FEA >> 16) & 0xff);
		if (_verbose)
			printf("Selected address (I): 0x%x 0x%x\n", tx[0], tx[1]);
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);

		/* ISC ERASE */
		printInfo("Flash erase: ", false);
		if (flashErase(FLASH_SEC_FEA) == false) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}

		/* FEATURE Row */
		printInfo("Program Feature Row: ", true);
		if (!programFeatureRow_MachXO3D(feature_row)) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}

		/* FEAbits */
		printInfo("Program FEAbits: ", true);
		if (!programFeabits_MachXO3D(feabits)) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
	}

	/* ISC program done 0x5E */
	printInfo("Write program Done: ", false);
	if (writeProgramDone() == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

	/* disable configuration mode */
	printInfo("Disable configuration: ", false);
	if (!DisableISC()) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	return true;
}

bool Lattice::program_intFlash_MachXO3D(JedParser& _jed)
{
	uint32_t erase_op = 0, prog_op = 0;
	std::vector<std::string> data;
	int offset, fuse_count;

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 with operand of 0x08 (Enable Offline mode) */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* this is the size of an CFGx+UFMx area in bits (hence the / 128) */
	fuse_count = _jed.get_fuse_count() / 128;

	for (size_t i = 0; i < _jed.nb_section(); i++) {
		std::string area_name;

		data = _jed.data_for_section(i);
		if (data.size() < 1) {
			/* if no data, nothing to do */
			continue;
		}
		std::string note = _jed.noteForSection(i);
		offset = _jed.offset_for_section(i) / 128;

		erase_op = 0;
		prog_op = 0;

		/* if the offset > total fuse count, then this file must be configured
		 * for the 2nd config sector (CFG1), so adjust offset */
		while (offset >= fuse_count) {
			offset -= fuse_count;
		}
		/* If the offset is greater than the size of the config area then we're
		 * programming the UFM area (UFM 0/1) */
		if (offset >= 12542) {
			offset -= 12542;
		}

		if (note == "END CONFIG DATA") {
			printf("Processing PADDING data (offset: %d (0x%x))\n", offset, offset);
			/* PADDING - this should be padding to CFGx area that we're currently
			 * programming therefore the flash area will already have been erased...
			 * NOTE: We have to write this data if we're using bitstream authentication
			 * - even if it's all zeros.
			 */
			erase_op = 0;
			/* We need to use the 'LSC_WRITE_ADDRESS' command to set not only the
			 * flash sector but also the page number. */
			if (_flash_sector == LATTICE_FLASH_CFG0) {
				prog_op = (FLASH_SET_ADDR_CFG0 << 14) | (offset);
				area_name = "Padding (CFG0)";
			} else if (_flash_sector == LATTICE_FLASH_CFG1) {
				prog_op = (FLASH_SET_ADDR_CFG1 << 14) | (offset);
				area_name = "Padding (CFG1)";
			}

			/* offset should not be zero */
			if (offset == 0) {
				printf("Warning: offset (%d) is for programming PADDING\n", offset);
			}
		} else if (note == "EBR_INIT DATA") {
			printf("Processing EBR_INIT data (offset: %d (0x%x))\n", offset, offset);
			/* EBR - Embedded Block RAM initialisation data */
			if (offset == 0) {
				if (_flash_sector == LATTICE_FLASH_CFG0) {
					erase_op = FLASH_SEC_UFM0;
					prog_op = FLASH_UFM_ADDR_UFM0;
					area_name = "EBR (UFM0)";
				} else if (_flash_sector == LATTICE_FLASH_CFG1) {
					erase_op = FLASH_SEC_UFM1;
					prog_op = FLASH_UFM_ADDR_UFM1;
					area_name = "EBR (UFM1)";
				}
			} else {
				/* NOT SUPPORTING NON-ZERO OFFSET WRITES...*/
				continue;
			}
		} else if (note.compare(0, 16, "USER MEMORY DATA") == 0) {
			printf("Processing UFM data (offset: %d (0x%x))\n", offset, offset);
			if ((_flash_sector == LATTICE_FLASH_CFG0)||
						(_flash_sector == LATTICE_FLASH_UFM0)) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM0;
					prog_op = FLASH_UFM_ADDR_UFM0;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM0 << 14) | (offset);
				}
				area_name = "UFM0";
			} else if ((_flash_sector == LATTICE_FLASH_CFG1)||
							(_flash_sector == LATTICE_FLASH_UFM1)) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM1;
					prog_op = FLASH_UFM_ADDR_UFM1;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM1 << 14) | (offset);
				}
				area_name = "UFM1";
			} else if (_flash_sector == LATTICE_FLASH_UFM2) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM2;
					prog_op = FLASH_UFM_ADDR_UFM2;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM2 << 14) | (offset);
				}
				area_name = "UFM2";
			} else if (_flash_sector == LATTICE_FLASH_UFM3) {
				if (offset == 0) {
					erase_op = FLASH_SEC_UFM3;
					prog_op = FLASH_UFM_ADDR_UFM3;
				} else {
					erase_op = 0;
					/* We need to use the 'LSC_WRITE_ADDRESS' command to set the
					 * flash sector and the page number. */
					prog_op = (FLASH_SET_ADDR_UFM3 << 14) | (offset);
				}
				area_name = "UFM3";
			}
		} else {
			printf("Processing CFG data (offset: %d (0x%x))\n", offset, offset);
			if (_flash_sector == LATTICE_FLASH_CFG0) {
				erase_op = FLASH_SEC_CFG0;
				prog_op = FLASH_SEC_CFG0;
				area_name = "Data (CFG0)";
			} else if (_flash_sector == LATTICE_FLASH_CFG1) {
				erase_op = FLASH_SEC_CFG1;
				prog_op = FLASH_SEC_CFG1;
				area_name = "Data (CFG1)";
			}

			/* offset should be zero */
			if (offset != 0) {
				printf("Warning: offset (%d) is not 0 for programming CFG\n", offset);
			}
		}

		if (erase_op > 0) {
			/* ISC ERASE */
			printInfo("Flash erase: ", false);
			if (flashErase(erase_op) == false) {
				printError("FAIL");
				return false;
			}
			printSuccess("DONE");
		}

		if (offset == 0) {
			/* LSC_INIT_ADDRESS */
			uint8_t tx[2] = {
				(uint8_t)((prog_op >> 8) & 0xff),
				(uint8_t)((prog_op >> 16) & 0xff)
			};
			printf("address (I): 0x%x 0x%x\n", tx[0], tx[1]);
			wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
		} else {
			/* LSC_WRITE_ADDRESS */
			uint8_t tx[3] = {
				(uint8_t)(prog_op & 0xff),
				(uint8_t)((prog_op >> 8) & 0xff),
				(uint8_t)((prog_op >> 16) & 0x03)
			};
			printf("address (W): 0x%x 0x%x 0x%x\n", tx[0], tx[1], tx[2]);
			wr_rd(LSC_WRITE_ADDRESS, tx, 3, NULL, 0);
		}
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);

		/* flash CfgFlash */
		if (false == flashProg(0, area_name, data))
			return false;

		/* verify write */
		if (_verify) {
			if (Verify(data, false, prog_op) == false)
				return false;
		}
	}

	/* @TODO: missing usercode update */

	/* LSC_INIT_ADDRESS */
	if (_flash_sector == LATTICE_FLASH_CFG0) {
		uint8_t tx[2] = {
			(uint8_t)((FLASH_SEC_CFG0 >> 8) & 0xff),
			(uint8_t)((FLASH_SEC_CFG0 >> 16) & 0xff)
		};
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
	} else if (_flash_sector == LATTICE_FLASH_CFG1) {
		uint8_t tx[2] = {
			(uint8_t)((FLASH_SEC_CFG1 >> 8) & 0xff),
			(uint8_t)((FLASH_SEC_CFG1 >> 16) & 0xff)
		};
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
	}
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	/* ISC program done 0x5E */
	printInfo("Write program Done: ", false);
	if (writeProgramDone() == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

	/* disable configuration mode */
	printInfo("Disable configuration: ", false);
	if (!DisableISC()) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	return true;
}

bool Lattice::program_pubkey_MachXO3D()
{
	bool err, same = true;
	int len, i, j;
	uint8_t pubkey[PUBKEY_LENGTH_BYTES];
	uint8_t rxkey[PUBKEY_LENGTH_BYTES];

	RawParser _pk(_filename, false);
	printInfo("Open file: ", false);
	printSuccess("DONE");

	err = _pk.parse();
	printInfo("Parse file: ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	const uint8_t* data = _pk.getData();
	len =  _pk.getLength()/8;

	if (data[0] == 0x0f && data[1] == 0xf0) {
		for (i = 2; i < len; i++) {
			if (data[i] == 0xf0 && data[i+1] == 0x0f) {
				if (_verbose) printf("Header: [%.*s]\n", i-2, ((char *)data)+2);
				i+=2;
				break;
			}
		}

		memcpy(pubkey, data+i, PUBKEY_LENGTH_BYTES);
		i += PUBKEY_LENGTH_BYTES;
/*
		As read from file:
		...
		7dbc273a6e614a0f5289070524a1a59d
		3a5d518b5cff00bc521f1ef62c4227ce
		dd7987ecb63768e3310864f4b44daf90
		ebf86ce8a9b17842821551a85b2235cc
		...

		As Sent by diamond programmer:
		0x59: cc35225ba85115824278b1a9e86cf8eb
		0x5B: 90af4db4f4640831e36837b6ec8779dd
		0x61: ce27422cf61e1f52bc00ff5c8b515d3a
		0x63: 9da5a124050789520f4a616e3a27bc7d
*/
		if (_verbose) {
			printf("PubKey: [");
			for (j=0; j < PUBKEY_LENGTH_BYTES; j++) {
				if (j && (j%16 == 0)) printf(" ");
				printf("%02x", pubkey[j]);
			}
			printf("]\n");
			printf("Trailing bytes: [");
			for (; i < len; i++) {
				printf("%02x ", data[i]);
			}
			printf("\b]\n");
		}
	}
	else {
		printError("Failed to find header in public key file");
		return false;
	}


	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 with operand of 0x08 (Enable Offline mode) */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	/* read the current feature row */
	wr_rd(READ_ECDSA_PUBKEY0, NULL, 0, rxkey, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(READ_ECDSA_PUBKEY1, NULL, 0, rxkey + 16, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(READ_ECDSA_PUBKEY2, NULL, 0, rxkey + 32, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(READ_ECDSA_PUBKEY3, NULL, 0, rxkey + 48, 16);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	if (_verbose) {
		printf("Read PubKey: [");
		for (j=PUBKEY_LENGTH_BYTES-1; j >= 0; j--) {
			printf("%02x", rxkey[j]);
			if (j && (j%16 == 0)) printf(" ");
		}
		printf("]\n");
	}

	for (int i = 0; i < PUBKEY_LENGTH_BYTES; i++) {
		if (pubkey[i] != rxkey[PUBKEY_LENGTH_BYTES - i - 1])
			same = false;
	}

	printf("PubKey Compare: %s\n", same ? "Same" : "Different");
	if (same == false) {
		uint8_t tx[2];
		/* LSC_INIT_ADDRESS */
		tx[0] = (uint8_t)((FLASH_SEC_PKEY >> 8) & 0xff);
		tx[1] = (uint8_t)((FLASH_SEC_PKEY >> 16) & 0xff);
		if (_verbose)
			printf("Selected address (I): 0x%x 0x%x\n", tx[0], tx[1]);
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);

		/* ISC ERASE */
		printInfo("Flash erase: ", false);
		if (flashErase(FLASH_SEC_PKEY) == false) {
			printError("FAIL");
			return false;
		}
		else {
			printSuccess("DONE");
		}

		/* Public Key */
		printInfo("Program Public Key: ", true);
		if (!programPubKey_MachXO3D(pubkey)) {
			printError("FAIL");
			return false;
		}
		else {
			printSuccess("DONE");
		}
	}

	/* Programming and Verify the AUTH_EN2 and AUTH_EN1 Fuses..."
	 * -- This is undocumented (extracted from USB capture) */
	uint8_t tx_byte = 0x03;
	wr_rd(0xc4, &tx_byte, 1, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

	/* lattice diamond sends this twice... ? */
	wr_rd(0xc4, &tx_byte, 1, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2);

	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

	if (_verbose) {
		wr_rd(READ_STATUS_REGISTER_1, NULL, 0, rxkey, 4);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		printf("Auth Mode: [%s] (0x%x)\n", (rxkey[1] & 0x03 ? "ECDSA Signature Verification" : rxkey[1] & 0x01 ? "HMAC Authentication" : "No Authentication"), rxkey[1] & 0x03);
	}

	/* ISC program done 0x5E */
	printInfo("Write program Done: ", false);
	if (writeProgramDone() == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(ISC_NOOP, NULL, 0, NULL, 0);

	/* disable configuration mode */
	printInfo("Disable configuration: ", false);
	if (!DisableISC()) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	return true;
}
