// Xilinx XC2C flow programming
#include "vendors/xilinx.hpp"
#include "utils/progressBar.hpp"
#include "vendors/xilinxMapParser.hpp"



#define XC2C_IDCODE         0x01
#define XC2C_ISC_DISABLE    0xc0
#define XC2C_VERIFY         0xd1
#define XC2C_ISC_ENABLE_OTF 0xe4
#define XC2C_ISC_WRITE      0xe6
#define XC2C_ISC_SRAM_READ  0xe7
#define XC2C_ISC_ENABLE     0xe8
#define XC2C_ISC_PROGRAM    0xea
#define XC2C_ISC_ERASE      0xed
#define XC2C_ISC_READ       0xee
#define XC2C_ISC_INIT       0xf0
#define XC2C_USERCODE       0xfd
#define BYPASS              0xFF
/* xilinx programmer qualification specification 6.2
 * directly reversed
 */
static constexpr uint8_t _gray_code[256] = {
	0x00, 0x80, 0xc0, 0x40, 0x60, 0xe0, 0xa0, 0x20,
	0x30, 0xb0, 0xf0, 0x70, 0x50, 0xd0, 0x90, 0x10,
	0x18, 0x98, 0xd8, 0x58, 0x78, 0xf8, 0xb8, 0x38,
	0x28, 0xa8, 0xe8, 0x68, 0x48, 0xc8, 0x88, 0x08,
	0x0c, 0x8c, 0xcc, 0x4c, 0x6c, 0xec, 0xac, 0x2c,
	0x3c, 0xbc, 0xfc, 0x7c, 0x5c, 0xdc, 0x9c, 0x1c,
	0x14, 0x94, 0xd4, 0x54, 0x74, 0xf4, 0xb4, 0x34,
	0x24, 0xa4, 0xe4, 0x64, 0x44, 0xc4, 0x84, 0x04,
	0x06, 0x86, 0xc6, 0x46, 0x66, 0xe6, 0xa6, 0x26,
	0x36, 0xb6, 0xf6, 0x76, 0x56, 0xd6, 0x96, 0x16,
	0x1e, 0x9e, 0xde, 0x5e, 0x7e, 0xfe, 0xbe, 0x3e,
	0x2e, 0xae, 0xee, 0x6e, 0x4e, 0xce, 0x8e, 0x0e,
	0x0a, 0x8a, 0xca, 0x4a, 0x6a, 0xea, 0xaa, 0x2a,
	0x3a, 0xba, 0xfa, 0x7a, 0x5a, 0xda, 0x9a, 0x1a,
	0x12, 0x92, 0xd2, 0x52, 0x72, 0xf2, 0xb2, 0x32,
	0x22, 0xa2, 0xe2, 0x62, 0x42, 0xc2, 0x82, 0x02,
	0x03, 0x83, 0xc3, 0x43, 0x63, 0xe3, 0xa3, 0x23,
	0x33, 0xb3, 0xf3, 0x73, 0x53, 0xd3, 0x93, 0x13,
	0x1b, 0x9b, 0xdb, 0x5b, 0x7b, 0xfb, 0xbb, 0x3b,
	0x2b, 0xab, 0xeb, 0x6b, 0x4b, 0xcb, 0x8b, 0x0b,
	0x0f, 0x8f, 0xcf, 0x4f, 0x6f, 0xef, 0xaf, 0x2f,
	0x3f, 0xbf, 0xff, 0x7f, 0x5f, 0xdf, 0x9f, 0x1f,
	0x17, 0x97, 0xd7, 0x57, 0x77, 0xf7, 0xb7, 0x37,
	0x27, 0xa7, 0xe7, 0x67, 0x47, 0xc7, 0x87, 0x07,
	0x05, 0x85, 0xc5, 0x45, 0x65, 0xe5, 0xa5, 0x25,
	0x35, 0xb5, 0xf5, 0x75, 0x55, 0xd5, 0x95, 0x15,
	0x1d, 0x9d, 0xdd, 0x5d, 0x7d, 0xfd, 0xbd, 0x3d,
	0x2d, 0xad, 0xed, 0x6d, 0x4d, 0xcd, 0x8d, 0x0d,
	0x09, 0x89, 0xc9, 0x49, 0x69, 0xe9, 0xa9, 0x29,
	0x39, 0xb9, 0xf9, 0x79, 0x59, 0xd9, 0x99, 0x19,
	0x11, 0x91, 0xd1, 0x51, 0x71, 0xf1, 0xb1, 0x31,
	0x21, 0xa1, 0xe1, 0x61, 0x41, 0xc1, 0x81, 0x01,
};

void Xilinx::xc2c_init(uint32_t idcode)
{
	_fpga_family = XC2C_FAMILY;
	std::string model = fpga_list[idcode].model;
	size_t underscore_pos = model.find_first_of('_', 0);
	if (underscore_pos == model.npos) {
		underscore_pos = model.length();
	}
	snprintf(_cpld_base_name, underscore_pos,
			"%s", model.substr(0, underscore_pos).c_str());
	switch ((idcode >> 16) & 0x3f) {
	case 0x01: /* xc2c32 */
	case 0x11: /* xc2c32a PC44 */
	case 0x21: /* xc2c32a */
		_cpld_nb_col = 260;
		_cpld_nb_row = 48;
		_cpld_addr_size = 6;
		break;
	case 0x05: /* xc2c64 */
	case 0x25: /* xc2c64a */
		_cpld_nb_col = 274;
		_cpld_nb_row = 96;
		_cpld_addr_size = 7;
		break;
	case 0x18: /* xc2c128 */
		_cpld_nb_col = 752;
		_cpld_nb_row = 80;
		_cpld_addr_size = 7;
		break;
	case 0x14: /* xc2c256 */
		_cpld_nb_col = 1364;
		_cpld_nb_row = 96;
		_cpld_addr_size = 7;
		break;
	case 0x15: /* xc2c384 */
		_cpld_nb_col = 1868;
		_cpld_nb_row = 120;
		_cpld_addr_size = 7;
		break;
	case 0x17: /* xc2c512 */
		_cpld_nb_col = 1980;
		_cpld_nb_row = 160;
		_cpld_addr_size = 8;
		break;
	default:
		throw std::runtime_error("Error: unknown XC2C version");
	}
	_cpld_nb_row += 2;  // 2 more row: done + sec and usercode
						// datasheet table 2 p.15
}

/* reinit device
 * datasheet table 47-48 p.61-62
 */
void Xilinx::xc2c_flow_reinit()
{
	uint8_t c = 0;
	_jtag->shiftIR(XC2C_ISC_ENABLE_OTF, 8);
	_jtag->shiftIR(XC2C_ISC_INIT, 8);
	_jtag->toggleClk((_jtag->getClkFreq() * 20) / 1000);
	_jtag->shiftIR(XC2C_ISC_INIT, 8);
	_jtag->shiftDR(&c, NULL, 8);
	_jtag->toggleClk((_jtag->getClkFreq() * 800) / 1000);
	_jtag->shiftIR(XC2C_ISC_DISABLE, 8);
	_jtag->shiftIR(BYPASS, 8);
}

/* full flash erase (with optional blank check)
 * datasheet 12.1 (table41) p.56
 */
bool Xilinx::xc2c_flow_erase()
{
	_jtag->shiftIR(XC2C_ISC_ENABLE_OTF, 8, Jtag::UPDATE_IR);
	_jtag->shiftIR(XC2C_ISC_ERASE, 8);
	_jtag->toggleClk((_jtag->getClkFreq() * 100) / 1000);
	_jtag->shiftIR(XC2C_ISC_DISABLE, 8);

	if (_verify) {
		std::string rx_buf = xc2c_flow_read();
		for (auto &val : rx_buf) {
			if ((uint8_t)val != 0xff) {
				printError("Erase: fails to verify blank check");
				return false;
			}
		}
	}

	return true;
}

/* read flash full content
 * return it has string buffer
 * table 45 - 46 p. 59-60
 */
std::string Xilinx::xc2c_flow_read()
{
	uint8_t rx_buf[249];
	uint32_t delay_loop = (_jtag->getClkFreq() * 20) / 1000000;
	uint16_t pos = 0;
	uint8_t addr_shift = 8 - _cpld_addr_size;

	std::string buffer;
	buffer.resize(((_cpld_nb_col * _cpld_nb_row) + 7) / 8);

	ProgressBar progress("Read Flash", _cpld_nb_row + 1, 50, _quiet);

	_jtag->shiftIR(BYPASS, 8);
	_jtag->shiftIR(XC2C_ISC_ENABLE_OTF, 8);
	_jtag->shiftIR(XC2C_ISC_READ, 8);

	/* send address
	 * send addr 0 before loop because each row content
	 * is followed by next addr (or dummy for the last row
	 */
	/* send address */
	uint8_t addr = _gray_code[0] >> addr_shift;
	_jtag->shiftDR(&addr, NULL, _cpld_addr_size);
	/* wait 20us */
	_jtag->toggleClk(delay_loop);

	for (size_t row = 1; row <= _cpld_nb_row; row++) {
		/* read nb_col bits, stay in shift_dr to send next addr */
		_jtag->shiftDR(NULL, rx_buf, _cpld_nb_col, Jtag::SHIFT_DR);
		/* send address */
		addr = _gray_code[row] >> addr_shift;
		_jtag->shiftDR(&addr, NULL, _cpld_addr_size);
		/* wait 20us */
		_jtag->toggleClk(delay_loop);

		for (int i = 0; i < _cpld_nb_col; i++, pos++)
			if (rx_buf[i >> 3] & (1 << (i & 0x07)))
				buffer[pos >> 3] |= (1 << (pos & 0x07));
			else
				buffer[pos >> 3] &= ~(1 << (pos & 0x07));

		progress.display(row);
	}
	progress.done();

	_jtag->shiftIR(XC2C_ISC_DISABLE, Jtag::TEST_LOGIC_RESET);

	return buffer;
}

bool Xilinx::xc2c_flow_program(JedParser *jed)
{
	uint32_t delay_loop = (_jtag->getClkFreq() * 20) / 1000;
	uint8_t shift_addr = 8 - _cpld_addr_size;

	/* map jed fuse using device map */
	printInfo("Map jed fuses: ", false);
	XilinxMapParser *map_parser;
	try {
		std::string mapname = ISE_DIR "/ISE_DS/ISE/xbr/data/" +
			std::string(_cpld_base_name) + ".map";
		map_parser = new XilinxMapParser(mapname, _cpld_nb_row, _cpld_nb_col,
				jed, 0xffffffff, _verbose);
		map_parser->parse();
	} catch(std::exception &e) {
		printError("FAIL");
		throw std::runtime_error(e.what());
	}
	printSuccess("DONE");

	std::vector<std::string> listfuse = map_parser->cfg_data();

	/* erase internal flash */
	printInfo("Erase Flash: ", false);
	if (!xc2c_flow_erase()) {
		printError("FAIL");
		throw std::runtime_error("Fail to erase interface flash");
	} else {
		printSuccess("DONE");
	}

	ProgressBar progress("Write Flash", _cpld_nb_row, 50, _quiet);

	_jtag->shiftIR(XC2C_ISC_ENABLE_OTF, 8);
	_jtag->shiftIR(XC2C_ISC_PROGRAM, 8);

	uint16_t iter = 0;
	for (const auto &row : listfuse) {
		uint16_t pos = 0;
		uint8_t addr = _gray_code[iter] >> shift_addr;
		uint8_t wr_buf[249] = {0};  // largest section length
		for (auto col : row) {
			if (col)
				wr_buf[pos >> 3] |= (1 << (pos & 0x07));
			else
				wr_buf[pos >> 3] &= ~(1 << (pos & 0x07));
			pos++;
		}
		_jtag->shiftDR(wr_buf, NULL, _cpld_nb_col, Jtag::SHIFT_DR);
		_jtag->shiftDR(&addr, NULL, _cpld_addr_size);
		_jtag->toggleClk(delay_loop);

		iter++;
	}

	/* done bit and usercode are shipped into listfuse
	 * so only needs to send isc disable
	 */
	_jtag->shiftIR(XC2C_ISC_DISABLE, 8);

	if (_verify) {
		std::string rx_buffer = xc2c_flow_read();
		iter = 0;
		for (const auto &row : listfuse) {
			for (auto col : row) {
				if ((rx_buffer[iter >> 3] >> (iter & 0x07)) != col) {
					throw std::runtime_error("Program: verify failed");
				}
				iter++;
			}
		}
	}

	/* reload */
	xc2c_flow_reinit();

	return true;
}

/*               */
/* SPI interface */
/*               */

