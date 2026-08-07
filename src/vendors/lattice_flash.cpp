// Lattice flash dump and programming operations
#include "vendors/lattice.hpp"
#include "utils/progressBar.hpp"
#include <cstring>
#include <cinttypes>
static bool machxo_internal_flash_layout(const std::string &model,
		uint32_t *cfg_pages, uint32_t *ufm_pages)
{
	if (model.find("9400") != std::string::npos) {
		*cfg_pages = 12539;
		*ufm_pages = 3582;
	} else if (model.find("6900") != std::string::npos) {
		*cfg_pages = 9212;
		*ufm_pages = 2046;
	} else if (model.find("4300") != std::string::npos ||
			model.find("4000") != std::string::npos) {
		*cfg_pages = 5758;
		*ufm_pages = 767;
	} else if (model.find("2100") != std::string::npos ||
			model.find("2000") != std::string::npos) {
		*cfg_pages = 3198;
		*ufm_pages = 639;
	} else if (model.find("1300") != std::string::npos ||
			model.find("1200") != std::string::npos) {
		*cfg_pages = 2175;
		*ufm_pages = 511;
	} else if (model.find("640") != std::string::npos) {
		*cfg_pages = 1151;
		*ufm_pages = 191;
	} else {
		return false;
	}

	return true;
}



bool Lattice::dumpFlash(uint32_t base_addr, uint32_t len)
{
	if (_fpga_family == MACHXO2_FAMILY || _fpga_family == MACHXO3_FAMILY)
		return dump_intFlash(base_addr, len);

	return FlashInterface::dump(base_addr, len);
}

bool Lattice::bulk_erase_flash()
{
	uint32_t erase_mask = FLASH_ERASE_ALL;
	std::string area = "ALL";

	if (_fpga_family == MACHXO2_FAMILY || _fpga_family == MACHXO3_FAMILY) {
		switch (_flash_sector) {
		case LATTICE_FLASH_UNDEFINED:
		case LATTICE_FLASH_ALL:
			erase_mask = FLASH_ERASE_ALL;
			area = "ALL";
			break;
		case LATTICE_FLASH_CFG:
		case LATTICE_FLASH_CFG0:
			erase_mask = FLASH_ERASE_CFG;
			area = "CFG";
			break;
		case LATTICE_FLASH_UFM:
		case LATTICE_FLASH_UFM0:
			erase_mask = FLASH_ERASE_UFM;
			area = "UFM";
			break;
		case LATTICE_FLASH_FEATURE:
		case LATTICE_FLASH_FEA:
			erase_mask = FLASH_ERASE_FEATURE;
			area = "FEATURE";
			break;
		case LATTICE_FLASH_SRAM:
			erase_mask = FLASH_ERASE_SRAM;
			area = "SRAM";
			break;
		default:
			printError("Error: selected flash sector is not valid for MachXO2/MachXO3");
			return false;
		}

		if (area == "ALL")
			printWarn("Erasing ALL internal flash also erases feature bits and SRAM");

		if (!EnableISC(ISC_ENABLE_FLASH_MODE)) {
			printError("Failed to enable ISC flash mode");
			return false;
		}

		printInfo("Internal flash erase " + area + ": ", false);
		const bool ret = flashErase(erase_mask);
		if (ret)
			printSuccess("DONE");
		else
			printError("FAIL");

		return DisableISC() && ret;
	}

	return FlashInterface::bulk_erase_flash();
}

bool Lattice::dump_intFlashPages(FILE *fd, const std::string &name,
		uint32_t area_base, uint32_t pages, uint32_t dump_base,
		uint32_t dump_len)
{
	const uint32_t page_size = 16;
	const uint32_t dump_end = dump_base + dump_len;
	uint8_t tx_buf[page_size], rx_buf[page_size];

	memset(tx_buf, 0, page_size);
	memset(rx_buf, 0, page_size);

	tx_buf[0] = REG_CFG_FLASH;
	_jtag->shiftIR(tx_buf, NULL, 8, Jtag::PAUSE_IR);

	ProgressBar progress("Reading " + name, pages, 50, _quiet);
	for (uint32_t page = 0; page < pages; page++) {
		const uint32_t page_base = area_base + page * page_size;
		const uint32_t page_end = page_base + page_size;

		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
		_jtag->shiftDR(tx_buf, rx_buf, page_size * 8, Jtag::PAUSE_DR);

		if (page_end > dump_base && page_base < dump_end) {
			const uint32_t first = std::max(page_base, dump_base);
			const uint32_t last = std::min(page_end, dump_end);
			const uint32_t page_offset = first - page_base;
			const uint32_t wr_len = last - first;
			const size_t written = fwrite(rx_buf + page_offset, 1, wr_len, fd);
			if (written != wr_len) {
				progress.fail();
				printError("Failed to write internal flash dump: " +
						std::string(strerror(errno)));
				return false;
			}
		}

		progress.display(page);
	}

	progress.done();
	return true;
}

bool Lattice::dump_intFlash(uint32_t base_addr, uint32_t len)
{
	const uint32_t page_size = 16;
	uint32_t cfg_pages = 0;
	uint32_t ufm_pages = 0;
	const std::string model = fpga_list[_jtag->get_target_device_id()].model;

	if (!machxo_internal_flash_layout(model, &cfg_pages, &ufm_pages)) {
		printError("Error: internal flash dump size is unknown for " + model);
		printError("       use a JEDEC verify flow, or add this device density layout");
		return false;
	}

	const uint32_t cfg_bytes = cfg_pages * page_size;
	const uint32_t ufm_bytes = ufm_pages * page_size;
	const uint32_t flash_bytes = cfg_bytes + ufm_bytes;
	if (base_addr >= flash_bytes) {
		printError("Error: internal flash dump offset is outside device flash");
		return false;
	}
	if (len == 0)
		len = flash_bytes - base_addr;

	if (len == 0 || len > flash_bytes - base_addr) {
		printError("Error: internal flash dump range is outside device flash");
		return false;
	}

	char content[160];
	snprintf(content, sizeof(content),
			"Dump internal Lattice flash: %u bytes (CFG %u pages, UFM %u pages)",
			len, cfg_pages, ufm_pages);
	printInfo(content);

	printInfo("Open dump file ", false);
	FILE *fd = fopen(_filename.c_str(), "wb");
	if (!fd) {
		printError("FAIL: " + std::string(strerror(errno)));
		return false;
	}
	printSuccess("DONE");

	bool ret = true;
	if (!EnableISC(ISC_ENABLE_FLASH_MODE)) {
		printError("Failed to enable ISC flash mode");
		fclose(fd);
		remove(_filename.c_str());
		return false;
	}

	wr_rd(RESET_CFG_ADDR, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	if (base_addr < cfg_bytes)
		ret = dump_intFlashPages(fd, "CFG", 0, cfg_pages, base_addr, len);

	if (ret && (base_addr + len) > cfg_bytes && ufm_pages > 0) {
		uint8_t tx[4] = {0, 0, 0, 0x40};
		wr_rd(LSC_WRITE_ADDRESS, tx, 4, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);

		ret = dump_intFlashPages(fd, "UFM", cfg_bytes, ufm_pages,
				base_addr, len);
	}

	if (!DisableISC())
		ret = false;

	if (fclose(fd) != 0) {
		printError("Failed to close internal flash dump: " +
				std::string(strerror(errno)));
		ret = false;
	}

	if (!ret)
		remove(_filename.c_str());

	return ret;
}

void displayFeabits(uint16_t _featbits)
{
	uint8_t boot_sequence = (_featbits >> 12) & 0x03;
	uint8_t m = (_featbits >> 11) & 0x01;
	printf("\tboot mode                                :");
	switch (boot_sequence) {
		case 0:
			if (m != 0x01)
				printf(" Single Boot from NVCM/Flash\n");
			else
				printf(" Dual Boot from NVCM/Flash then External if there is a failure\n");
			break;
		case 1:
			if (m == 0x01)
				printf(" Single Boot from External Flash\n");
			else
				printf(" Error!\n");
			break;
		default:
			printf(" Error!\n");
	}
	printf("\tMaster Mode SPI                          : %s\n",
		(((_featbits>>11)&0x01)?"enable":"disable"));
	printf("\tI2c port                                 : %s\n",
		(((_featbits>>10)&0x01)?"disable":"enable"));
	printf("\tSlave SPI port                           : %s\n",
		(((_featbits>>9)&0x01)?"disable":"enable"));
	printf("\tJTAG port                                : %s\n",
		(((_featbits>>8)&0x01)?"disable":"enable"));
	printf("\tDONE                                     : %s\n",
		(((_featbits>>7)&0x01)?"enable":"disable"));
	printf("\tINITN                                    : %s\n",
		(((_featbits>>6)&0x01)?"enable":"disable"));
	printf("\tPROGRAMN                                 : %s\n",
		(((_featbits>>5)&0x01)?"disable":"enable"));
	printf("\tMy_ASSP                                  : %s\n",
		(((_featbits>>4)&0x01)?"enable":"disable"));
	printf("\tPassword (Flash Protect Key) Protect All : %s\n",
		(((_featbits>>3)&0x01)?"Enabled" : "Disabled"));
	printf("\tPassword (Flash Protect Key) Protect     : %s\n",
		(((_featbits>>2)&0x01)?"Enabled" : "Disabled"));
}

bool Lattice::checkStatus(uint64_t val, uint64_t mask)
{
	uint64_t reg = readStatusReg();

	return ((reg & mask) == val) ? true : false;
}

/* PRELOAD/SAMPLE 0x1C
 * For NEXUS family fpgas, the Bscan register is 362 bits long or
 * 45.25 bytes => 46 bytes
 * For ECP3 family fpgas, the Bscan register is 1077 bits long or
 * 134.62 bytes => 135 bytes
 */
bool Lattice::preload()
{
    uint8_t tx_buf[135];
    memset(tx_buf, 0xff, 135);
    int tx_len;
    int tx_bit_len;
	switch (_fpga_family) {
		case ECP3_FAMILY:
			tx_len = 135;
			tx_bit_len = 1077;
			break;
		case NEXUS_FAMILY:
			tx_len = 46;
			tx_bit_len = 362;
			break;
		default:
			tx_len = 26;
			tx_bit_len = tx_len * 8;
	}
    if(_fpga_family == NEXUS_FAMILY){
        uint8_t cmd = PRELOAD_SAMPLE;
        _jtag->shiftIR(&cmd, NULL, 8, Jtag::RUN_TEST_IDLE);
        _jtag->shiftDR(tx_buf, NULL, tx_bit_len,
            Jtag::RUN_TEST_IDLE);
    } else {
        wr_rd(PRELOAD_SAMPLE, tx_buf, tx_len, NULL, 0);
    }

	return true;
}

bool Lattice::program_mem()
{
	bool err;

	LatticeBitParser _bit(_filename, false, false, _verbose);

	printInfo("Open file: ", false);
	printSuccess("DONE");

	err = _bit.parse();

	printInfo("Parse file: ", false);
	if (err == EXIT_FAILURE) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_verbose)
		_bit.displayHeader();

	/* read ID Code 0xE0 and compare to bitstream */
	uint32_t bit_idcode = std::stoul(_bit.getHeaderVal("idcode").c_str(), NULL, 16);
	uint32_t idcode = idCode();
	if (idcode != bit_idcode) {
		char mess[256];
		snprintf(mess, 256, "mismatch between target's idcode and bitstream idcode\n"
			"\tbitstream has 0x%08X hardware requires 0x%08x", bit_idcode, idcode);
		printError(mess);
		return false;
	}

	if (_verbose) {
		printf("IDCode : %x\n", idcode);
		displayReadReg(readStatusReg());
	}

	/* The command code 0x1C is not listed in the manual?
	 * PRELOAD/SAMPLE 0x1C
	 * For NEXUS family fpgas, the Bscan register is 362 bits long or
	 * 45.25 bytes => 46 bytes
	 * For ECP3 family fpgas, the Bscan register is 1077 bits long or
	 * 134.62 bytes => 135 bytes
	 */
	preload();

	/* LSC_REFRESH 0x79 -- "Equivalent to toggle PROGRAMN pin"
	 * We REFRESH only if the fpga is in a status of error due to
	 * the previous bitstream. For example, this happens if
	 * no bitstream is present on the SPI FLASH
	 */
	/*flag to understand if we refreshed or not*/
	bool was_refreshed;
	switch (_fpga_family) {
		case NEXUS_FAMILY:
			//if (!checkStatus(0, REG_STATUS_PRV_CNF_CHK_MASK)) {
			//	printInfo("Error in previous bitstream execution. REFRESH: ", false);
			//	wr_rd(REFRESH, NULL, 0, NULL, 0);
			//	_jtag->set_state(Jtag::RUN_TEST_IDLE);
			//	_jtag->toggleClk(1000);
			//	/* In Lattice FPGA-TN-02099 document in a note it's reported that there
			//		 is a delay time after LSC_REFRESH where "Duration could be in
			//		 seconds". Without whis waiting time, busy flag can't be cleared.*/
			//	sleep(5);
			//	was_refreshed = true;
			//	if (!checkStatus(0, REG_STATUS_PRV_CNF_CHK_MASK)) {
			//		printError("FAIL");
			//		displayReadReg(readStatusReg());
			//		return false;
			//	} else {
			//		printSuccess("DONE");
			//	}
			//} else {
			//	was_refreshed = false;
			//	if (_verbose){
			//		printInfo("No error in previous bitstream execution.", true);
			//	}
			//}
			break;
		case ECP3_FAMILY:
			wr_rd(ECP3_LSCC_REFRESH, NULL, 0, NULL, 0);
			_jtag->set_state(Jtag::RUN_TEST_IDLE);
			_jtag->toggleClk(5, 1);
			usleep_ecp3(500000);  // 0.5s
			was_refreshed = false;
			break;
		default:
			was_refreshed = false;
	}

	/* ISC Enable 0xC6 */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x00)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_fpga_family == NEXUS_FAMILY) {
		wr_rd(REFRESH, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);

		/* LSC_DEVICE_CONTROL 0x7D -- configuration reset */
		printInfo("Configuration Logic Reset: ", false);
		uint8_t tx_tmp[1] = {0x08};
		wr_rd(LSC_DEVICE_CONTROL, tx_tmp, 1, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);
		if(!pollBusyFlag()) {
			printError("FAIL");
			return false;
		}

		tx_tmp[0] = 0x00;
		wr_rd(LSC_DEVICE_CONTROL, tx_tmp, 1, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);
		if(!pollBusyFlag()) {
			printError("FAIL");
			return false;
		}
		printSuccess("DONE");
	}

	if (_fpga_family == ECP3_FAMILY) {
		if (!write_userCode(0xffffffff))
			return false;
	}

	/* ISC ERASE
	 * For Nexus family (from svf file): 1 byte to tx 0x00
	 */
	printInfo("SRAM erase: ", false);
	uint32_t mask_erase[1] = {FLASH_ERASE_SRAM};
	if (_fpga_family == NEXUS_FAMILY){
		mask_erase[0] = 0x00;
	}
	if (flashErase(mask_erase[0]) == false) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_fpga_family == NEXUS_FAMILY) {
		printInfo("SRAM erase check: ", false);
		uint64_t status = readStatusReg();
		if ((status & 0x000000000002100) != 0) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
	}

	/* LSC_INIT_ADDRESS */
	if (_fpga_family == ECP3_FAMILY) {
		wr_rd(ECP3_RESET_ADDRESS, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(5);
		usleep_ecp3(2000); // 2ms

		/* read user code (User Code register must have all bit set cleared) */
		const uint32_t dummy = 0xffffffff;
		uint32_t rx = 0;
		wr_rd(ECP3_READ_USERCODE, (uint8_t *)&dummy, 4, (uint8_t *)&rx, 4);
		if (rx != 0x00000000) {
			char message[256];
			snprintf(message, 256, "failed: 0x%08x instead of 0xffffffff", rx);
			printError(message);
			return false;
		}
	} else {
		wr_rd(RESET_CFG_ADDR, NULL, 0, NULL, 0); // LSC_INIT_ADDRESS
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);
	}

	const uint8_t *data = _bit.getData();
	int length = _bit.getLength()/8;
	if (_fpga_family == ECP3_FAMILY) {
		/* Reset Address */
		wr_rd(ECP3_RESET_ADDRESS, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(5, 1);
		usleep_ecp3(2000); // 2ms

		wr_rd(ECP3_LSCC_BITSTREAM_BURST, NULL, 0, NULL, 0);
	} else {
		wr_rd(LSC_BITSTREAM_BURST, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	}

	uint8_t tmp[1024];
	int size = 1024;
	Jtag::tapState_t next_state = Jtag::SHIFT_DR;

	ProgressBar progress("Loading", length, 50, _quiet);

	for (int i = 0; i < length; i += size) {
		progress.display(i);

		if (length < i + size) {
			size = length-i;
			next_state = Jtag::RUN_TEST_IDLE;
		}

		for (int ii = 0; ii < size; ii++)
			tmp[ii] = ConfigBitstreamParser::reverseByte(data[i+ii]);

		_jtag->shiftDR(tmp, NULL, size * 8, next_state);
	}

	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	if (_fpga_family == ECP3_FAMILY) {
		_jtag->toggleClk(256, 1);
		usleep_ecp3(2000);

		/* Verifiy User Code register */
		uint32_t rx = userCode();
		if (rx != 0x00000000) {
			progress.fail();
			char message[256];
			snprintf(message, 256, "failed: 0x%08x instead of 0x00000000", rx);
			printError(message);
			return false;
		}
		progress.done();
	} else {
		_jtag->toggleClk(1000);

		uint32_t status_mask;
		uint32_t status_val = 0;
		if (_fpga_family == MACHXO3D_FAMILY)
			status_mask = REG_STATUS_MACHXO3D_CNF_CHK_MASK;
		else if (_fpga_family == NEXUS_FAMILY) {
			status_val = REG_STATUS_DONE;
			status_mask = REG_STATUS_DONE | REG_STATUS_BUSY | REG_STATUS_FAIL
				| REG_NEXUS_STATUS_BSE_ERR_MASK;
		}
		else
			status_mask = REG_STATUS_CNF_CHK_MASK;

		if (checkStatus(status_val, status_mask)) {
			progress.done();
		} else {
			progress.fail();
			displayReadReg(readStatusReg());
			return false;
		}

		/* bypass */
		wr_rd(0xff, NULL, 0, NULL, 0);
	}

	if (_verbose)
		printf("userCode: %08x\n", userCode());

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	/* disable configuration mode */
	printInfo("Disable configuration: ", false);
	if (!DisableISC()) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_fpga_family == ECP3_FAMILY) {
		/* Bypass */
		wr_rd(0xff, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(100, 1);
		usleep_ecp3(1000);

		// Verify STATUS Register
		uint64_t status = readStatusReg();
		_jtag->toggleClk(5, 1);
		if ((status & 0x60007) != 0x20000) {
			char message[256];
			snprintf(message, 256, "Programming failed: status 0x%" PRIx64 "instead of 0x20000", status);
			printError(message);
			displayReadReg(status);
			return false;
		}
	}

	if (_verbose)
		displayReadReg(readStatusReg());

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	if (_fpga_family == NEXUS_FAMILY) {
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(100);
		usleep_ecp3(1000000);
	}
	_jtag->go_test_logic_reset();
	return true;
}

