// Lattice register and flash operations
#include "vendors/lattice.hpp"
#include "utils/progressBar.hpp"
#include <cstring>
#include <iomanip>
#include <cinttypes>

bool Lattice::wr_rd(uint8_t cmd,
					uint8_t *tx, int tx_len,
					uint8_t *rx, int rx_len,
					bool verbose)
{
	int kXferLen = rx_len;
	if (tx_len > rx_len)
		kXferLen = tx_len;

	uint8_t xfer_tx[kXferLen];
	uint8_t xfer_rx[kXferLen];
	memset(xfer_tx, 0, kXferLen);
	int i;
	if (tx != NULL && tx_len > 0) {
		for (i = 0; i < tx_len; i++)
			xfer_tx[i] = tx[i];
	}

	_jtag->shiftIR(&cmd, NULL, 8, Jtag::PAUSE_IR);
	if (rx || tx) {
		_jtag->shiftDR(xfer_tx, (rx) ? xfer_rx : NULL, 8 * kXferLen,
			Jtag::PAUSE_DR);
	}
	if (rx) {
		if (verbose) {
			for (i=kXferLen-1; i >= 0; i--)
				printf("%02x ", xfer_rx[i]);
			printf("\n");
		}
		for (i = 0; i < rx_len; i++)
			rx[i] = (xfer_rx[i]);
	}
	return true;
}

#define REG_ENTRY(_description, _offset, _size, ...) \
	{_description, _offset, _size, {__VA_ARGS__}}

const std::map<int, std::map<std::string, std::list<Lattice::reg_struct_t>>> Lattice::reg_content = {
	{ECP3_FAMILY, {
		{"StatusRegister", std::list<reg_struct_t>{
			REG_ENTRY("CRC Error",            0, 1, {0, "OK"}, {1, "KO"}),
			REG_ENTRY("ID Verify failed",     1, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Invalid Command",      2, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("SPIm Mode Error",      3, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Device locked",        4, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Key Fire",             5, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Alignement Preamble",  6, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Encryption Preamble",  7, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Std Preamble",         8, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("HFC",                  9, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Memory Cleared",      15, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Device Secured",      16, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Device Configured",   17, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Non-JTAG Mode",       18, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("ReadBack Mode",       19, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Bypass Mode",         20, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Flow Trough",         21, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Configuration Mode",  22, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Transparent Mode",    23, 1, {0, "No"}, {1, "Yes"}),
		}},
	}},
	{NEXUS_FAMILY, {
		{"StatusRegister", std::list<reg_struct_t>{
			REG_ENTRY("Transparent Mode",        0, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Cfg Target Select",       1, 3,
				{0, "SRAM array"},
				{1, "EFUSE Normal"},
				{2, "EFUSE Pseudo;"},
				{3, "EFUSE Safe"}),
			REG_ENTRY("JTAG Active",             4, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("PWD Protection",          5, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("OTP",                     6, 1, {0, "No"}, {1, "Yes"}),
			/* [7] Reserved */
			REG_ENTRY("DONE",                    8, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("ISC Enable",              9, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Write Enable",           10, 1,
				{0, "Not Writable"}, {1, "Writable"}),
			REG_ENTRY("Read Enable",            11, 1,
				{0, "Not Readable"}, {1, "Readable"}),
			REG_ENTRY("Busy Flag",              12, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Fail Flag",              13, 1, {0, "No"}, {1, "Yes"}),
			/* [14] Reserved */
			REG_ENTRY("Decrypt Only",           15, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("PWD Enable",             16, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("PWD All",                17, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("CID EN",                 18, 1, {0, "No"}, {1, "Yes"}),
			/* [19] Unused */
			/* [20] Reserved */
			REG_ENTRY("Encrypt Preamble",       21, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("STD Preamble",           22, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("SPIm Fail 1",            23, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("BSE Error Code",         24, 4,
				{ 0, "No error"},
				{ 1, "ID error"},
				{ 2, "CMD error - illegal command detected"},
				{ 3, "CRC error"},
				{ 4, "PRMB error - preamble error.         "},
				{ 5, "ABRT error - configuration is aborted"},
				{ 6, "OVFL error - data overflow error.     "},
				{ 7, "SDM error - bitstream pass the size of the SRAM array"},
				{ 8, "Authentication Error1"},
				{ 9, "Authentication Setup Error1"},
				{10, "Bitstream Engine Timeout Error"}),
			REG_ENTRY("Execution Error",        28, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("ID Error",               29, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Invalid Command",        30, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("WDT Busy",               31, 1, {0, "No"}, {1, "Yes"}),
			/* [32] Reserved */
			REG_ENTRY("Dry Run DONE",           33, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("BSE Error 1 Code",       34, 4,
				{ 0, "No error"},
				{ 1, "ID error"},
				{ 2, "CMD error - illegal command detected"},
				{ 3, "CRC error"},
				{ 4, "PRMB error - preamble error"},
				{ 5, "ABRT error - configuration is aborted"},
				{ 6, "OVFL error - data overflow error"},
				{ 7, "SDM error - bitstream pass the size of the SRAM array"},
				{ 8, "Authentication Error1"},
				{ 9, "Authentication Setup Error1"},
				{10, "Bitstream Engine Timeout Error"}),
			REG_ENTRY("Bypass Mode",            38, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Flow Through Mode",      39, 1, {0, "No"}, {1, "Yes"}),
			/* [40-41] Reserved */
			REG_ENTRY("SFDP Timeout",           42, 1, {0, "No"},  {1, "Yes"}),
			REG_ENTRY("Key Destroy Pass",       43, 1, {0, "No"},  {1, "Yes"}),
			REG_ENTRY("INITN",                  44, 1, {0, "Low"}, {1, "High"}),
			REG_ENTRY("I3C Parity Error 2",     45, 1, {0, "No"},  {1, "Yes"}),
			REG_ENTRY("INIT Bus ID Error",      46, 1, {0, "No"},  {1, "Yes"}),
			REG_ENTRY("I3C Parity Error 1",     47, 1, {0, "No"},  {1, "Yes"}),

			REG_ENTRY("Authentication Mode",    48, 2,
				{0, "No Auth"},
				{1, "ECDSA"},
				{2, "HMAC"},
				{3, "No Auth"}),
			REG_ENTRY("Authentication Done",    50, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Dry Run Auth Done",      51, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("JTAG Locked",            52, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("SSPI Locked",            53, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("I2C/I3C Locked",         54, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("PUB Read Lock",          55, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("PUB Write Lock",         56, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("FEA Read Lock",          57, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("FEA Write Lock",         58, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("AES Read Lock",          59, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("AES Write Lock",         60, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("PWD Read Lock",          61, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("PWD Write Lock",         62, 1, {0, "No"}, {1, "Yes"}),
			REG_ENTRY("Global Lock",            63, 1, {0, "No"}, {1, "Yes"}),
		}},
	}},
};

void Lattice::displayReadReg(uint64_t dev)
{
	if (_fpga_family == ECP3_FAMILY || _fpga_family == NEXUS_FAMILY) {
		auto rc = reg_content.find(_fpga_family);
		if (rc == reg_content.end()) {
			printError("Unknown family");
			return;
		}
		auto reg = rc->second.find("StatusRegister");
		if (reg == rc->second.end()) {
			printError("Unknown register StatusRegister");
			return;
		}

		std::stringstream raw_val;
		raw_val << "0x" << std::hex << dev;
		printSuccess("Register raw value: " + raw_val.str());

		const std::list<reg_struct_t> &regs = reg->second;
		for (reg_struct_t r: regs) {
			uint8_t offset = r.offset;
			uint8_t size = r.size;
			uint32_t mask = (1 << size) - 1;
			uint32_t val = (dev >> offset) & mask;
			std::stringstream ss, desc;
			desc << r.description;
			ss << std::setw(20) << std::left << r.description;
			if (r.reg_cnt.size() != 0) {
				ss << r.reg_cnt[val];
			} else {
				std::stringstream hex_val;
				hex_val << "0x" << std::hex << val;
				ss << hex_val.str();
			}

			printInfo(ss.str());
		}
		return;
	}

	uint8_t err;
	printf("displayReadReg\n");
	if (dev & 1<<0) printf("\tTRAN Mode\n");
	printf("\tConfig Target Selection : %" PRIx64 "\n", (dev >> 1) & 0x07);
	if (dev & 1<<4) printf("\tJTAG Active\n");
	if (dev & 1<<5) printf("\tPWD Protect\n");
	if (dev & 1<<6) printf("\tOTP\n");
	if (dev & 1<<7) printf("\tDecrypt Enable\n");
	if (dev & REG_STATUS_DONE) printf("\tDone Flag\n");
	if (dev & REG_STATUS_ISC_EN) printf("\tISC Enable\n");
	if (dev & 1 << 10) printf("\tWrite Enable\n");
	if (dev & 1 << 11) printf("\tRead Enable\n");
	if (dev & REG_STATUS_BUSY) printf("\tBusy Flag\n");
	if (dev & REG_STATUS_FAIL) printf("\tFail Flag\n");
	if (dev & 1 << 14) printf("\tFFEA OTP\n");
	if (dev & 1 << 15) printf("\tDecrypt Only\n");
	if (dev & 1 << 16) printf("\tPWD Enable\n");
	if (dev & 1 << 17) printf("\tUFM OTP\n");
	if (dev & 1 << 18) printf("\tASSP\n");
	if (dev & 1 << 19) printf("\tSDM Enable\n");
	if (dev & 1 << 20) printf("\tEncryption PreAmble\n");
	if (dev & 1 << 21) printf("\tStd PreAmble\n");
	if (dev & 1 << 22) printf("\tSPIm Fail1\n");
	err = (dev >> 23)&0x07;

	printf("\tBSE Error Code\n");
	printf("\t\t");
	switch (err) {
		case 0:
			printf("No err\n");
			break;
		case 1:
			printf("ID ERR\n");
			break;
		case 2:
			printf("CMD ERR\n");
			break;
		case 3:
			printf("CRC ERR\n");
			break;
		case 4:
			printf("Preamble ERR\n");
			break;
		case 5:
			printf("Abort ERR\n");
			break;
		case 6:
			printf("Overflow ERR\n");
			break;
		case 7:
			printf("SDM EOF\n");
			break;
		case 8:
			printf("Authentication ERR\n");
			break;
		case 9:
			printf("Authentication Setup ERR\n");
			break;
		case 10:
			printf("Bitstream Engine Timeout ERR\n");
			break;
		default:
			printf("unknown error: %x\n", err);
	}

	if (dev & REG_STATUS_EXEC_ERR) printf("\tEXEC Error\n");
	if ((dev >> 27) & 0x01) printf("\tDevice failed to verify\n");
	if ((dev >> 28) & 0x01) printf("\tInvalid Command\n");
	if ((dev >> 29) & 0x01) printf("\tSED Error\n");
	if ((dev >> 30) & 0x01) printf("\tBypass Mode\n");
	if ((dev >> 31) & 0x01) printf("\tFT Mode\n");
}

bool Lattice::pollBusyFlag(bool verbose)
{
	uint8_t rx;
	int timeout = 0;
	do {
		wr_rd(READ_BUSY_FLAG, NULL, 0, &rx, 1);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);
		if (verbose)
			printf("pollBusyFlag :%02x\n", rx);
		if (timeout == 100000000){
			std::cerr << "timeout" << std::endl;
			return false;
		} else {
			timeout++;
		}
	} while (rx != 0);

	return true;
}

bool Lattice::flashEraseAll()
{
	return flashErase(0xf);
}

bool Lattice::flashErase(uint32_t  mask)
{
	if (_fpga_family == MACHXO3D_FAMILY) {
		uint8_t tx[2] = {
			(uint8_t)((mask >> 8) & 0xff),
			(uint8_t)((mask >> 16) & 0xff)
		};
		wr_rd(FLASH_ERASE, tx, 2, NULL, 0);
	} else if (_fpga_family == ECP3_FAMILY) {
		wr_rd(ECP3_ISC_ERASE, NULL, 0, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(5, 1);
		usleep_ecp3(2000000);  // 2s
		return true;
	} else {
		uint8_t tx[1] = {(uint8_t)(mask & 0xff)};
		wr_rd(FLASH_ERASE, tx, 1, NULL, 0);
	}
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	if (_fpga_family == NEXUS_FAMILY) {
		_jtag->toggleClk(100);
		usleep_ecp3(1000000);
	} else {
		_jtag->toggleClk(1000);
	}

	if (!pollBusyFlag())
		return false;

	if (!checkStatus(0, REG_STATUS_FAIL))
		return false;

	return true;
}

bool Lattice::flashProg(uint32_t start_addr, const std::string &name, std::vector<std::string> data)
{
	(void)start_addr;
	ProgressBar progress("Writing " + name, data.size(), 50, _quiet);
	for (uint32_t line = 0; line < data.size(); line++) {
		wr_rd(PROG_CFG_FLASH, (uint8_t *)data[line].c_str(),
				16, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);
		progress.display(line);
		if (pollBusyFlag() == false)
			return false;
	}
	progress.done();
	return true;
}

bool Lattice::Verify(std::vector<std::string> data, bool unlock, uint32_t flash_area)
{
	uint8_t tx_buf[16], rx_buf[16];
	if (unlock)
		EnableISC(0x08);

	if (_fpga_family == MACHXO3D_FAMILY) {
		uint8_t tx[2] = { (
			uint8_t)((flash_area >> 8) & 0xff),
			(uint8_t)((flash_area >> 16) & 0xff)
		};
		wr_rd(RESET_CFG_ADDR, tx, 2, NULL, 0);
	} else {
		wr_rd(RESET_CFG_ADDR, NULL, 0, NULL, 0);
	}

	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	tx_buf[0] = REG_CFG_FLASH;
	_jtag->shiftIR(tx_buf, NULL, 8, Jtag::PAUSE_IR);

	memset(tx_buf, 0, 16);
	bool failure = false;
	ProgressBar progress("Verifying", data.size(), 50, _quiet);
	for (size_t line = 0;  line< data.size(); line++) {
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(2);
		_jtag->shiftDR(tx_buf, rx_buf, 16*8, Jtag::PAUSE_DR);
		for (size_t i = 0; i < data[line].size(); i++) {
			if (rx_buf[i] != (unsigned char)data[line][i]) {
				printf("%3zu %3zu %02x -> %02x\n", line, i,
						rx_buf[i], (unsigned char)data[line][i]);
				failure = true;
			}
		}
		if (failure) {
			printf("Verify Failure\n");
			break;
		}
		progress.display(line);
	}
	if (unlock)
		DisableISC();

	if (failure)
		progress.fail();
	else
		progress.done();

	return !failure;
}

uint64_t Lattice::readFeaturesRow()
{
	uint8_t tx_buf[8];
	uint8_t rx_buf[8];
	uint64_t reg = 0;
	memset(tx_buf, 0, 8);
	wr_rd(READ_FEATURE_ROW, tx_buf, 8, rx_buf, 8);
	for (int i = 0; i < 8; i++)
		reg |= ((uint64_t)rx_buf[i] << (i*8));
	return reg;
}

uint16_t Lattice::readFeabits()
{
	uint8_t rx_buf[2];
	wr_rd(READ_FEABITS, NULL, 0, rx_buf, 2);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	return rx_buf[0] | (((uint16_t)rx_buf[1]) << 8);
}

bool Lattice::writeFeaturesRow(uint64_t features, bool verify)
{
	uint8_t tx_buf[8];
	for (int i=0; i < 8; i++)
		tx_buf[i] = ((features >> (i*8)) & 0x00ff);
	wr_rd(PROG_FEATURE_ROW, tx_buf, 8, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (verify)
		return (features == readFeaturesRow()) ? true : false;
	return true;
}

bool Lattice::writeFeabits(uint16_t feabits, bool verify)
{
	uint8_t tx_buf[2] = {(uint8_t)(feabits&0x00ff),
							(uint8_t)(0x00ff & (feabits>>8))};

	wr_rd(PROG_FEABITS, tx_buf, 2, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (verify)
		return (feabits == readFeabits()) ? true : false;
	return true;
}

bool Lattice::writeProgramDone()
{
	wr_rd(PROG_DONE, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(REG_STATUS_DONE, REG_STATUS_DONE))
		return false;
	return true;
}

bool Lattice::loadConfiguration()
{
	wr_rd(REFRESH, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);
	if (!pollBusyFlag())
		return false;
	if (!checkStatus(REG_STATUS_DONE, REG_STATUS_DONE))
		return false;
	return true;
}

uint16_t Lattice::getUFMStartPageFromJEDEC(JedParser *_jed, int id)
{
	/* In general, Lattice tools try to fill UFM from the highest
	page to lowest. JEDEC files will give a starting bit offset. */
	uint32_t bit_offset = _jed->offset_for_section(id);
	/* Convert to starting page, relative to Configuration Flash's page 0.
	For 7000 parts only, first UFM page starts 16 bytes (1 page) after
		the last Configuration Flash page, based on looking at
		Diamond-generated JEDECs.

		For all other parts, the first UFM page immediately follows the last
		Configuration Flash page. */
	uint16_t raw_page_offset = bit_offset / 128;

	/* Raw page offsets won't overlap- see Lattice TN-02155, page 49. So we
	can uniquely determine which part type we're targeting from the UFM start
	addres.
	TODO: In any case, JEDEC files don't carry part information. Verify against
	IDCODE read previously? */

	if(raw_page_offset >= 12539) {
		return raw_page_offset - 12539; // 9400
	} else if(raw_page_offset > 9211) {
		return raw_page_offset - 9211 - 1; // 7000
	} else if(raw_page_offset > 5758) {
		return raw_page_offset - 5758; // 4000, 2000U
	} else if(raw_page_offset > 3198) {
		return raw_page_offset - 3198; // 2000, 1200U
	} else if(raw_page_offset > 2175) {
		return raw_page_offset - 2175; // 1200, 640U
	} else if(raw_page_offset > 1151) {
		return raw_page_offset - 1151; // 640
	} else {
		// 256- We should bail if we get here! No UFM!
		return 0xffff;
	}
}

/* ------------------ */
/* SPI implementation */
/* ------------------ */
