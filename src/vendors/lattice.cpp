// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#define __STDC_FORMAT_MACROS
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <list>
#include <stdexcept>
#include <vector>

#include "protocols/jtag.hpp"
#include "vendors/lattice.hpp"
#include "vendors/latticeBitParser.hpp"
#include "parsers/mcsParser.hpp"
#include "utils/progressBar.hpp"
#include "parsers/rawParser.hpp"
#include "utils/display.hpp"
#include "utils/part.hpp"
#include "protocols/spiFlash.hpp"



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

Lattice::Lattice(Jtag *jtag, const std::string filename, const std::string &file_type,
	Device::prog_type_t prg_type, std::string flash_sector, bool verify, int8_t verbose, bool skip_load_bridge, bool skip_reset):
		Device(jtag, filename, file_type, verify, verbose),
		FlashInterface(filename, verbose, 0, verify, skip_load_bridge, skip_reset),
		_fpga_family(UNKNOWN_FAMILY), _flash_sector(LATTICE_FLASH_UNDEFINED)
{
	if (prg_type == Device::RD_FLASH) {
		_mode = READ_MODE;
	} else if (!_file_extension.empty()) {
		if (_file_extension == "jed" || _file_extension == "mcs" ||
				_file_extension == "fea" || _file_extension == "pub") {
			_mode = Device::FLASH_MODE;
		} else if (_file_extension == "bit" || _file_extension == "bin") {
			if (prg_type == Device::WR_FLASH)
				_mode = Device::FLASH_MODE;
			else
				_mode = Device::MEM_MODE;
		} else { /* unknown type: */
			if (prg_type == Device::WR_FLASH) /* to flash: OK */
				_mode = Device::FLASH_MODE;
			else /* otherwise: KO */
				throw std::runtime_error("incompatible file format");
		}
	}
	/* check device family */
	uint32_t idcode = _jtag->get_target_device_id();
	std::string family = fpga_list[idcode].family;
	if (family == "MachXO2") {
		_fpga_family = MACHXO2_FAMILY;
		set_flash_sector(flash_sector);
	} else if (family == "MachXO3L" || family == "MachXO3LF") {
		_fpga_family = MACHXO3_FAMILY;
		set_flash_sector(flash_sector);
	} else if (family == "MachXO3D") {
		_fpga_family = MACHXO3D_FAMILY;
		set_flash_sector(flash_sector);
	} else if (family == "ECP3") {
		_fpga_family = ECP3_FAMILY;
	} else if (family == "ECP5") {
		_fpga_family = ECP5_FAMILY;
	} else if (family == "CrosslinkNX") {
		_fpga_family = NEXUS_FAMILY;
	} else if (family == "CertusNX") {
		_fpga_family = NEXUS_FAMILY;
	} else if (family == "CertusProNX") {
		_fpga_family = NEXUS_FAMILY;
	} else {
		printError("Unknown device family");
		throw std::exception();
	}
}

void Lattice::set_flash_sector(const std::string &flash_sector)
{
	if (flash_sector.empty())
		return;

	if (flash_sector == "CFG") {
		_flash_sector = LATTICE_FLASH_CFG;
	} else if (flash_sector == "UFM") {
		_flash_sector = LATTICE_FLASH_UFM;
	} else if (flash_sector == "FEATURE" || flash_sector == "FEA") {
		_flash_sector = LATTICE_FLASH_FEATURE;
	} else if (flash_sector == "SRAM") {
		_flash_sector = LATTICE_FLASH_SRAM;
	} else if (flash_sector == "ALL") {
		_flash_sector = LATTICE_FLASH_ALL;
	} else if (flash_sector == "CFG0") {
		_flash_sector = LATTICE_FLASH_CFG0;
	} else if (flash_sector == "CFG1") {
		_flash_sector = LATTICE_FLASH_CFG1;
	} else if (flash_sector == "UFM0") {
		_flash_sector = LATTICE_FLASH_UFM0;
	} else if (flash_sector == "UFM1") {
		_flash_sector = LATTICE_FLASH_UFM1;
	} else if (flash_sector == "UFM2") {
		_flash_sector = LATTICE_FLASH_UFM2;
	} else if (flash_sector == "UFM3") {
		_flash_sector = LATTICE_FLASH_UFM3;
	} else if (flash_sector == "PKEY") {
		_flash_sector = LATTICE_FLASH_PKEY;
	} else if (flash_sector == "AKEY") {
		_flash_sector = LATTICE_FLASH_AKEY;
	} else {
		printError("Unknown flash sector");
		throw std::exception();
	}

	printInfo("Flash Sector: " + flash_sector, true);
}

bool Lattice::prepare_flash_access()
{
	if (_skip_load_bridge) {
		printInfo("Skip switching to SPI access");
		return true;
	}
	/* clear SRAM before SPI access */
	if (!clearSRAM())
		return false;
	if (_fpga_family == ECP3_FAMILY) {
		if (!wr_rd(0x3A, 0, 0, 0, 0, false))
			return false;
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
	} else {
		/*IR = 0h3A, DR=0hFE,0h68. Enter RUNTESTIDLE.
		 * thank @GregDavill
		 * https://twitter.com/GregDavill/status/1251786406441086977
		 */
		_jtag->shiftIR(0x3A, 8, Jtag::EXIT1_IR);
		uint8_t tmp[2] = {0xFE, 0x68};
		_jtag->shiftDR(tmp, NULL, 16);
	}
	return true;
}

bool Lattice::post_flash_access()
{
	bool ret = true, flash_blank = false;
	if (_skip_reset) {
		printInfo("Skip resetting device");
		return true;
	}
	if (_fpga_family == ECP3_FAMILY) {
		_jtag->shiftIR(0xFF, 8, Jtag::RUN_TEST_IDLE);
		_jtag->shiftIR(0x23, 8, Jtag::RUN_TEST_IDLE);
	} else {
		/* ISC REFRESH 0x79 */
		if (loadConfiguration() == false) {
			/* when flash is blank status displays failure:
			 * try to check flash first sector
			 */
			_skip_reset = true;  // avoid infinite loop
			/* read flash 0 -> 255 */
			uint8_t buffer[256];
			ret = FlashInterface::read(buffer, 0, 256);
			loadConfiguration(); // reset again

			/* read ok? check if everything == 0xff */
			if (ret) {
				for (int i = 0; i < 256; i++) {
					/* not blank: fail */
					if (buffer[i] != 0xFF) {
						ret = false;
						break;
					}
				}
				/* to add a note */
				flash_blank = true;
			}
		}

		printInfo("Refresh: ", false);
		if (!ret) {
			printError("FAIL");
			displayReadReg(readStatusReg());
			return false;
		} else {
			printSuccess("DONE");
			if (flash_blank)
				printWarn("Flash is blank");
		}

		/* bypass */
		wr_rd(0xff, NULL, 0, NULL, 0);
		_jtag->go_test_logic_reset();
	}
	return true;
}
void Lattice::reset()
{
	if (_fpga_family == ECP5_FAMILY || _fpga_family == ECP3_FAMILY
			|| _fpga_family == NEXUS_FAMILY)
		post_flash_access();
	else
		printError("Lattice Reset only tested on ECP5 Family.");
}

bool Lattice::clearSRAM()
{
	uint32_t erase_op;

	/* PRELOAD/SAMPLE 0x1C
	 * For NEXUS family fpgas, the Bscan register is 362 bits long or
	 * 45.25 bytes => 46 bytes
	 */
	uint8_t tx_buf[46];
	memset(tx_buf, 0xff, 46);
	int tx_len;
	int tx_bit_len;
	if(_fpga_family == NEXUS_FAMILY){
		tx_len = 46;
		tx_bit_len = 362;
		uint8_t cmd = PRELOAD_SAMPLE;
		_jtag->shiftIR(&cmd, NULL, 8, Jtag::RUN_TEST_IDLE);
		_jtag->shiftDR(tx_buf, NULL, tx_bit_len,
			Jtag::RUN_TEST_IDLE);
		wr_rd(REFRESH, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
	} else {
		tx_len = 26;
		tx_bit_len = 26 * 8;
		wr_rd(PRELOAD_SAMPLE, tx_buf, tx_len, NULL, 0);

		wr_rd(BYPASS, NULL, 0, NULL, 0);
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

	if (_fpga_family == MACHXO3D_FAMILY || _fpga_family == NEXUS_FAMILY)
		erase_op = 0x0;
	else
		erase_op = FLASH_ERASE_SRAM;

	/* ISC ERASE */
	printInfo("SRAM erase: ", false);
	if (flashErase(erase_op) == false) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	return DisableISC();
}

bool Lattice::program_extFlash(unsigned int offset, bool unprotect_flash)
{
	int ret;
	ConfigBitstreamParser *_bit;

	printInfo("Open file ", false);
	try {
		if (_file_extension == "mcs")
			_bit = new McsParser(_filename, true, _verbose);
		else if (_file_extension == "bit")
			_bit = new LatticeBitParser(_filename, false,
				_fpga_family==ECP3_FAMILY, _verbose);
		else
			_bit = new RawParser(_filename, false);
		printSuccess("DONE");
	} catch (std::exception &e) {
		printError("FAIL");
		printError(e.what());
		return false;
	}

	printInfo("Parse file ", false);
	if (_bit->parse() == EXIT_FAILURE) {
		printError("FAIL");
		delete _bit;
		return false;
	} else {
		printSuccess("DONE");
	}

	if (_verbose)
		_bit->displayHeader();

	if (_file_extension == "bit") {
		uint32_t bit_idcode = std::stoul(_bit->getHeaderVal("idcode").c_str(), NULL, 16);
		uint32_t idcode = idCode();
		if (idcode != bit_idcode) {
			char mess[256];
			snprintf(mess, 256, "mismatch between target's idcode and bitstream idcode\n"
				"\tbitstream has 0x%08X hardware requires 0x%08x", bit_idcode, idcode);
			printError(mess);
			delete _bit;
			return false;
		}
	}

	if (_file_extension == "mcs") {
		McsParser *parser = (McsParser *)_bit;
		ret = FlashInterface::write(parser->getRecords(), unprotect_flash, true);
	} else {
		ret = FlashInterface::write(offset, _bit->getData(), _bit->getLength() / 8,
			unprotect_flash);
	}

	delete _bit;
	return ret;
}

bool Lattice::program_flash(unsigned int offset, bool unprotect_flash)
{
	/* read ID Code 0xE0 */
	if (_verbose) {
		printf("IDCode : %x\n", idCode());
		displayReadReg(readStatusReg());
	}

	bool retval;
	if (_file_extension == "jed") {
		bool err;

		JedParser _jed(_filename, _verbose);
		printInfo("Open file ", false);
		printSuccess("DONE");

		err = _jed.parse();
		printInfo("Parse file ", false);
		if (err == EXIT_FAILURE) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
		if (_verbose)
			_jed.displayHeader();

		/* clear current SRAM content */
		clearSRAM();

		if (_fpga_family == MACHXO3D_FAMILY)
			retval = program_intFlash_MachXO3D(_jed);
		else
			retval = program_intFlash(
					reinterpret_cast<ConfigBitstreamParser*>(&_jed));
		/* for machXO2 and unlike TN02155 & TN1204 ISC_DISABLE is required
		 * and REFRESH no
		 * TODO: same for machXO3x ?
		 */
		if (_fpga_family == MACHXO2_FAMILY)
			return retval;

		return post_flash_access() && retval;
	} else if (_file_extension == "fea") {
		/* clear current SRAM content */
		clearSRAM();
		retval = program_fea_MachXO3D();
		return post_flash_access() && retval;
	} else if (_file_extension == "pub") {
		/* clear current SRAM content */
		clearSRAM();
		program_pubkey_MachXO3D();
	} else {
		//  machox2 + bit
		if (_file_extension == "bit" && _fpga_family == MACHXO2_FAMILY) {
			try {
				LatticeBitParser _bit(_filename, true, _verbose);
				_bit.parse();
				retval = program_intFlash(
						reinterpret_cast<ConfigBitstreamParser *>(&_bit));
			} catch (std::exception &e) {
				return false;
			}
			return post_flash_access() && retval;
		}
		/* !machXO and any file */
		return program_extFlash(offset, unprotect_flash);
	}

	return true;
}

void Lattice::program(unsigned int offset, bool unprotect_flash)
{
	bool retval = true;
	if (_mode == FLASH_MODE)
		retval = program_flash(offset, unprotect_flash);
	else if (_mode == MEM_MODE)
		retval = program_mem();
	if (!retval)
		throw std::exception();
}

/* flash mode :
 */
bool Lattice::EnableISC(uint8_t flash_mode)
{
	uint8_t cmd = ISC_ENABLE;

	if (_fpga_family == ECP3_FAMILY) {
		wr_rd(ECP3_ISC_ENABLE, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(5, 1);
		usleep_ecp3(20000); // 0.20s
		return true;
	}

	wr_rd(cmd, &flash_mode, 1, NULL, 0);

	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(REG_STATUS_ISC_EN, REG_STATUS_ISC_EN))
		return false;
	return true;
}

bool Lattice::DisableISC()
{
	if (_fpga_family == ECP3_FAMILY) {
		/** Shift in ISC DISABLE instruction */
		_jtag->shiftIR(0x1E, 8, Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(5, 1);
		usleep_ecp3(200000);
		return true;
	}
	wr_rd(ISC_DISABLE, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	if (_fpga_family == NEXUS_FAMILY) {
		_jtag->toggleClk(2);
		wr_rd(BYPASS, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(100);
	} else {
		_jtag->toggleClk(1000);
	}
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(0, REG_STATUS_ISC_EN))
		return false;
	return true;
}

bool Lattice::EnableCfgIf()
{
	uint8_t tx_buf = 0x08;
	wr_rd(0x74, &tx_buf, 1, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	return pollBusyFlag();
}

bool Lattice::DisableCfg()
{
	uint8_t tx_buf = 0, rx_buf;
	wr_rd(0x26, &tx_buf, 1, &rx_buf, 1);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	return true;
}

uint32_t Lattice::idCode()
{
	uint8_t device_id[4];
	const uint8_t idcode = (_fpga_family == ECP3_FAMILY) ? ECP3_IDCODE : READ_DEVICE_ID_CODE;
	wr_rd(idcode, NULL, 0, device_id, 4);
	return device_id[3] << 24 |
					device_id[2] << 16 |
					device_id[1] << 8  |
					device_id[0];
}

int Lattice::userCode()
{
	uint8_t usercode[4];
	wr_rd((_fpga_family == ECP3_FAMILY) ? ECP3_READ_USERCODE : 0xC0,
		NULL, 0, usercode, 4);
	return usercode[3] << 24 |
					usercode[2] << 16 |
					usercode[1] << 8  |
					usercode[0];
}

bool Lattice::write_userCode(uint32_t usercode)
{
	// ! Shift in ISC PROGRAM USERCODE(0x1A) instruction
	// SIR 8   TDI  (1A);
	// SDR 32  TDI  (FFFFFFFF);
	// RUNTEST IDLE    5 TCK   2,00E-03 SEC;
	// ! Shift in READ USERCODE(0x17) instruction
	// SIR 8   TDI  (17);
	// SDR 32  TDI  (FFFFFFFF)
	//         TDO  (FFFFFFFF);
	wr_rd(ECP3_ISC_PROGRAM_USERCODE, (uint8_t *)&usercode, 4, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(5, 1);
	usleep_ecp3(2000); // 2ms

	uint32_t rd_usercode = userCode();
	if (rd_usercode != usercode) {
		char message[256];
		snprintf(message, 256, "failed: 0x%08x instead of 0x%08x",
			rd_usercode, usercode);
		printError(message);
		return false;
	}
	return true;
}

bool Lattice::checkID()
{
	printf("\n");
	printf("check ID\n");
	uint8_t tx[4] = { 0 };
	wr_rd(0xE2, tx, 4, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	uint32_t reg = readStatusReg();
	displayReadReg(reg);

	tx[3] = 0x61;
	tx[2] = 0x2b;
	tx[1] = 0xd0;
	tx[0] = 0x43;
	wr_rd(0xE2, tx, 4, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	reg = readStatusReg();
	displayReadReg(reg);
	printf("%08x\n", reg);
	printf("\n");
	return true;
}

/* returns the number of bits of the Device Status Register
 * accordingly to _fpga_family
 */
int Lattice::get_statusreg_size(){
	if (_fpga_family == NEXUS_FAMILY) {
		return 64;
	} else{
		return 32;
	}
}

/* feabits is MSB first
 * maybe this register too
 * or not
 */
uint64_t Lattice::readStatusReg()
{
	uint64_t reg;
	uint8_t rx[8], tx[8];

	const int reg_len = get_statusreg_size() / 8;
	const uint8_t regcode = (_fpga_family == ECP3_FAMILY) ? ECP3_READ_STATUS_REGISTER : READ_STATUS_REGISTER;

	/* valgrind warn */
	memset(tx, 0, 8);
	memset(rx, 0, 8);
	wr_rd(regcode, tx, reg_len, rx, reg_len);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	reg = (uint64_t) rx[7] << 56 | (uint64_t) rx[6] << 48 | (uint64_t) rx[5] << 40 | (uint64_t) rx[4] << 32 | rx[3] << 24 | rx[2] << 16 | rx[1] << 8 | rx[0];
	return reg;
}


int Lattice::spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	const uint32_t xfer_len = len + 1 + ((rx != NULL) && ((_fpga_family == ECP3_FAMILY)) ? 1 : 0);
	const uint32_t xfer_bit_len = (len + 1) * 8 + ((rx != NULL) && ((_fpga_family == ECP3_FAMILY)) ? 1 : 0);
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];

	memset(jrx, 0, xfer_len);
	memset(jtx, 0, xfer_len);

	jtx[0] = LatticeBitParser::reverseByte(cmd);

	if (tx) {
		for (uint32_t i=0; i < len; i++)
			jtx[i+1] = LatticeBitParser::reverseByte(tx[i]);
	}

	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (!rx)? NULL: jrx, xfer_bit_len);

	if (rx) {
		if (_fpga_family == ECP3_FAMILY) {
			for (uint32_t i = 0; i < len; ++i) {
				const uint8_t tmp = LatticeBitParser::reverseByte((jrx[i+1] >> 1) & 0x7f);
				rx[i] = tmp | (jrx[i + 2] & 0x01);
			}
		} else {
			for (uint32_t i=0; i < len; i++)
				rx[i] = LatticeBitParser::reverseByte(jrx[i+1]);
		}
	}
	return 0;
}

int Lattice::spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	if (len == 0)
		return 0;
	uint8_t jtx[len];
	uint8_t jrx[len];

	memset(jrx, 0, len);

	if (tx)
		for (uint32_t i = 0; i < len; ++i)
			jtx[i] = LatticeBitParser::reverseByte(tx[i]);
	else
		memset(jtx, 0, len);

	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx) ? jrx : nullptr, 8 * len);

	if (rx) {
		for (uint32_t i = 0; i < len; ++i)
			rx[i] = LatticeBitParser::reverseByte(jrx[i]);
	}
	return 0;
}

int Lattice::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
		uint32_t timeout, bool verbose)
{
	uint8_t rx[2];
	uint8_t dummy[2] = {0xff};
	uint8_t tmp;
	uint8_t tx = LatticeBitParser::reverseByte(cmd);
	uint32_t count = 0;
	uint32_t nb_byte = (_fpga_family == ECP3_FAMILY) ? 2 : 1;

	/* CS is low until state goes to EXIT1_IR
	 * so manually move to state machine to stay is this
	 * state as long as needed
	 */
	_jtag->shiftDR(&tx, NULL, 8, Jtag::SHIFT_DR);

	do {
		_jtag->shiftDR(dummy, rx, 8 * nb_byte, Jtag::SHIFT_DR);
		if (_fpga_family == ECP3_FAMILY)
			tmp = LatticeBitParser::reverseByte(rx[0] >> 1) | (rx[1] & 0x01);
		else
			tmp = (LatticeBitParser::reverseByte(rx[0]));
		count++;
		if (count == timeout){
			printf("timeout: %x %x %u\n", tmp, rx[0], count);
			break;
		}

		if (verbose) {
			printf("%x %x %x %u\n", tmp, mask, cond, count);
		}
	} while ((tmp & mask) != cond);
	_jtag->shiftDR(dummy, rx, 8, Jtag::RUN_TEST_IDLE);
	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	}
	return 0;
}

/*************************** MODS FOR ECP3 ************************************/

void Lattice::usleep_ecp3(uint64_t us_time)
{
	const uint32_t clk_period = 1e9/static_cast<float>(_jtag->getClkFreq());
	const uint32_t clk_len = (us_time * 1000) / clk_period;
	_jtag->toggleClk(clk_len, 0); // 0.5s
}

/*************************** MODS FOR MacXO3D *********************************/
