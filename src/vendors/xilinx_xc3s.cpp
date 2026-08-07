// Xilinx XC3S flow programming
#include "vendors/xilinx.hpp"
#include "utils/progressBar.hpp"
#include <cstring>

#define BYPASS          0xff
#define XC95_IDCODE     0xfe
#define XC95_ISC_ERASE  0xed
#define XC95_ISC_ENABLE 0xe9
#define XC95_ISC_DISABLE 0xf0
#define XC95_XSC_BLANK_CHECK 0xe5
#define XC95_ISC_PROGRAM 0xea
#define XC95_ISC_READ   0xee
#define ISC_ENABLE  0x10
#define ISC_DISABLE 0x16
#define JPROGRAM    0x0B
#define JSTART      0x0C
#define JSHUTDOWN   0x0D
#define CFG_IN      0x05


/* flow program for xc3s (legacy mode)          */
/* based on ISE spartan3/data/xx_1532.bsd files */
/*                                              */

bool Xilinx::xc3s_flow_program(ConfigBitstreamParser *bit)
{
	int byte_length = bit->getLength() / 8;
	int burst_len = byte_length / 100;
	const uint8_t *data = bit->getData();
	int tx_len = burst_len * 8;
	Jtag::tapState_t tx_end = Jtag::SHIFT_DR;
	ProgressBar progress("Load SRAM", byte_length, 50, _quiet);

	flow_enable();

	if (_jtag->shiftIR(JPROGRAM, _irlen) < 0)
		return false;

	/* wait until memory cleared (DS099 v3.1 fig.30 p.52) */
	uint8_t tx_buf = BYPASS, rx_buf;
	do {
		if (_jtag->shiftIR(&tx_buf, &rx_buf, _irlen) < 0)
			return false;
	} while (!(rx_buf & 0x10));  // wait until INIT

	if (_jtag->shiftIR(JSHUTDOWN, _irlen) < 0)
		return false;
	_jtag->toggleClk(16);
	if (_jtag->shiftIR(CFG_IN, _irlen) < 0)
		return false;

	for (int i = 0; byte_length > 0; byte_length-=burst_len, data+=burst_len) {
		if (burst_len > byte_length) {
			tx_len = byte_length * 8;
			tx_end = Jtag::RUN_TEST_IDLE;
		}
		if (_jtag->shiftDR(data, NULL, tx_len, tx_end) < 0) {
			progress.fail();
			return false;
		}
		_jtag->flush();
		progress.display(i);
		i+= burst_len;
	}
	progress.done();
	_jtag->toggleClk(1);
	if (_jtag->shiftIR(JSTART, _irlen) < 0)
		return false;
	_jtag->toggleClk(32);
	if (_jtag->shiftIR(BYPASS, _irlen) < 0)
		return false;
	uint8_t d = 0;
	if (_jtag->shiftDR(&d, NULL, 1) < 0)
		return false;
	_jtag->toggleClk(1);

	flow_disable();
	uint8_t mask = 0x20;  // Done bit
	uint32_t idcode = _jtag->get_target_device_id();
	if (fpga_list[idcode].family == "spartan3e") {
		mask = 0x10;  // ISC done dit
	}
	int retry = 100;
	do {
		if (_jtag->shiftIR(&tx_buf, &rx_buf, _irlen) < 0)
			return false;
		if (_jtag->shiftDR(data, NULL, 1) < 0)
			return false;
	} while (!(rx_buf & mask) && (retry-- > 0));  // wait until mask

	return true;
}

void Xilinx::flow_enable()
{
	uint8_t xfer_buf = 0x15;
	uint8_t isc_enable = XC95_ISC_ENABLE;
	int drlen = 6, tcklen = 1;
	if (_fpga_family == SPARTAN3_FAMILY) {
		xfer_buf = 0x00;
		isc_enable = ISC_ENABLE;
		drlen = 5;
		tcklen = 16;
	}
	if (_jtag->shiftIR(isc_enable, _irlen) < 0)
		return;
	if (_jtag->shiftDR(&xfer_buf, NULL, drlen) < 0)
		return;
	_jtag->toggleClk(tcklen);
}

void Xilinx::flow_disable()
{
	uint8_t isc_disable = XC95_ISC_DISABLE;
	int tcklen = ((_jtag->getClkFreq() * 100) / 1000000);

	if (_fpga_family == SPARTAN3_FAMILY) {
		isc_disable = ISC_DISABLE;
		tcklen = 16;
	}

	if (_jtag->shiftIR(isc_disable, _irlen) < 0)
		return;
	_jtag->toggleClk(tcklen);
	if (_jtag->shiftIR(BYPASS, _irlen) < 0)
		return;
	if (_fpga_family == SPARTAN3_FAMILY) {
		uint8_t xfer_buf = 0;
		if (_jtag->shiftDR(&xfer_buf, NULL, 1) < 0)
			return;
	}
	_jtag->toggleClk(1);
}

/*                                              */
/* internal flash (xc95)                        */
/* based on ISE xc9500yy/data/xx_1532.bsd files */
/*                                              */

bool Xilinx::flow_erase()
{
	uint8_t xfer_buf[3] = {0x03, 0x00, 0x00};

	printInfo("Erase flash ", false);

	_jtag->shiftIR(XC95_ISC_ERASE, 8);
	_jtag->shiftDR(xfer_buf, NULL, 18);
	_jtag->toggleClk((_jtag->getClkFreq() * 400) / 1000);
	_jtag->shiftDR(NULL, xfer_buf, 18);
	if ((xfer_buf[0] & 0x03) != 0x01) {
		printError("FAIL");
		return false;
	}

	if (_verify) {
		xfer_buf[0] = 0x03;
		xfer_buf[1] = xfer_buf[2] = 0x00;

		_jtag->shiftIR(XC95_XSC_BLANK_CHECK, 8);
		_jtag->shiftDR(xfer_buf, NULL, 18);
		_jtag->toggleClk(500);
		_jtag->shiftDR(NULL, xfer_buf, 18);
		if ((xfer_buf[0] & 0x03) != 0x01) {
			printError("FAIL");
			return false;
		}
	}
	printSuccess("DONE");

	return true;
}

bool Xilinx::flow_program(JedParser *jed)
{
	uint8_t wr_buf[16+2];  // largest section length
	uint8_t rd_buf[16+3];

	/* enable ISC */
	flow_enable();

	/* erase internal flash */
	if (!flow_erase())
		return false;

	/* xc95 internal flash is written by sector
	 * for each one them 15 jed sections are used
	 */
	size_t nb_section = jed->nb_section() / (15);

	ProgressBar progress("Write Flash", nb_section, 50, _quiet);

	for (size_t i = 0; i < nb_section; i++) {
		uint16_t addr2 = i * 32;
		for (int ii = 0; ii < 15; ii++) {
			uint8_t mode = (ii == 14) ? 0x3 : 0x1;
			int id = i * 15 + ii;

			memcpy(wr_buf, jed->data_for_section(id)[0].c_str(),
					_xc95_line_len);
			wr_buf[_xc95_line_len] = (uint8_t) addr2&0xff;
			wr_buf[_xc95_line_len+ 1 ] = (uint8_t)((addr2 >> 8) & 0xff);

			_jtag->shiftIR(XC95_ISC_PROGRAM, 8);
			_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
			_jtag->shiftDR(wr_buf, NULL, 8 * (_xc95_line_len + 2));

			if (ii == 14)
				_jtag->toggleClk((_jtag->getClkFreq() * 50) / 1000);
			else
				_jtag->toggleClk(1);


			if (ii == 14) {
				mode = 0x00;
				for (int loop_try = 0; loop_try < 32; loop_try++) {
					_jtag->shiftIR(XC95_ISC_PROGRAM, 8);
					_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
					_jtag->shiftDR(wr_buf, NULL, 8 * (_xc95_line_len + 2));
					_jtag->toggleClk((_jtag->getClkFreq() * 50) / 1000);
					_jtag->shiftDR(NULL, rd_buf, 8 * (_xc95_line_len + 2) + 2);
					if ((rd_buf[0] & 0x03) == 0x01)
						break;
				}

				if ((rd_buf[0] & 0x03) != 0x01) {
					progress.fail();
					return false;
				}
			}
			addr2 += ((ii+1) % 0x05) ? 1 : 4;
		}
		progress.display(i);
	}
	progress.done();

	/* TODO: verify */
	if (_verify) {
		std::string flash = flow_read();
		int flash_pos = 0;
		ProgressBar progress2("Verify Flash", nb_section, 50, _quiet);
		for (size_t section = 0; section < nb_section; section++) {
			for (size_t subsection = 0; subsection < 15; subsection++) {
				int id = section * 15 + subsection;
				std::string content = jed->data_for_section(id)[0];
				for (int col = 0; col < _xc95_line_len; col++, flash_pos++) {
					if ((uint8_t)content[col] != (uint8_t)flash[flash_pos]) {
						char error[256];
						progress2.fail();
						snprintf(error, sizeof(error),
								"Error: wrong value: read %02x instead of %02x",
								(uint8_t)flash[flash_pos], (uint8_t)content[col]);
						printError(error);
						flow_disable();
						return false;
					}
				}
			}
		}
		progress2.done();
	}

	/* disable ISC */
	flow_disable();

	return true;
}

std::string Xilinx::flow_read()
{
	uint8_t mode;
	std::string buffer;
	uint8_t wr_buf[16+2];  // largest section length
	uint8_t rd_buf[16+2];
	memset(wr_buf, 0xff, sizeof(wr_buf));

	/* limit JTAG clock frequency to 1MHz */
	if (_jtag->getClkFreq() > 1e6)
		_jtag->setClkFreq(1e6);

	ProgressBar progress("Read Flash", 108, 50, _quiet);

	for (size_t section = 0; section < 108; section++) {
		uint16_t addr2 = section * 32;
		for (int subsection = 0; subsection < 15; subsection++) {
			wr_buf[_xc95_line_len    ] = (uint8_t)((addr2     ) & 0xff);
			wr_buf[_xc95_line_len + 1] = (uint8_t)((addr2 >> 8) & 0xff);

			mode = 3;
			_jtag->shiftIR(XC95_ISC_READ, 8);
			_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
			_jtag->shiftDR(wr_buf, NULL, 8 * (_xc95_line_len + 2));

			_jtag->toggleClk(1);

			mode = 0;
			_jtag->shiftDR(&mode, NULL, 2, Jtag::SHIFT_DR);
			_jtag->shiftDR(NULL, rd_buf, 8 * (_xc95_line_len + 2));
			for (int pos = 0; pos < _xc95_line_len; pos++)
				buffer += rd_buf[pos];
			addr2 += ((subsection+1) % 0x05) ? 1 : 4;
		}
		progress.display(section);
	}
	progress.done();

	return buffer;
}

/*               */
/*   XCF Prom    */
/*               */

#define XCF_FVFY3          0xE2
#define XCF_ISCTESTSTATUS  0xE3
#define XCF_ISC_ENABLE     0xE8
#define XCF_ISC_PROGRAM    0xEA
#define XCF_ISC_ADDR_SHIFT 0xEB
#define XCF_ISC_ERASE      0xEC
#define XCF_ISC_DATA_SHIFT 0xED
#define XCF_CONFIG         0xEE
#define XCF_ISC_READ       0xeF
#define XCF_ISC_DISABLE    0xF0
#define XCFP_ISC_READ      0xF8
#define XCFP_IDCODE        0x00FE
#define XCFP_BYPASS        0xFFFF
#define XCFP_XSC_UNLOCK    0xAA55
#define XCFP_XSC_DATA_BTC  0x00F2
#define XCFP_XSC_DATA_CCB  0x000C
#define XCFP_XSC_DATA_SUCR 0x000E
#define XCFP_XSC_DATA_DONE 0x0009

