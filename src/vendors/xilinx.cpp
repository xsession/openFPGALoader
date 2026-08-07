// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 * Copyright (C) 2026 Apple Inc.
 */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>
#include <unistd.h>

#include <algorithm>
#include <cstring>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "vendors/bitparser.hpp"
#include "utils/common.hpp"
#include "parsers/configBitstreamParser.hpp"
#include "utils/display.hpp"
#include "parsers/jedParser.hpp"
#include "protocols/jtag.hpp"
#include "parsers/mcsParser.hpp"
#include "utils/part.hpp"
#include "utils/progressBar.hpp"
#if defined (_WIN64) || defined (_WIN32)
#include "utils/pathHelper.hpp"
#endif
#include "parsers/rawParser.hpp"
#include "protocols/spiFlash.hpp"
#include "protocols/flashInterface.hpp"
#include "vendors/xilinx.hpp"
#include "vendors/xilinxMapParser.hpp"

namespace {

std::string yes_no(bool value)
{
	return value ? "yes" : "no";
}

std::string format_hex32(uint32_t value)
{
	std::ostringstream oss;
	oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << value;
	return oss.str();
}

std::string to_lower_copy(const std::string &value)
{
	std::string result = value;
	for (char &ch : result) {
		ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
	}
	return result;
}

bool file_exists(const std::string &filename)
{
	std::ifstream file(filename, std::ios::binary);
	return file.good();
}

std::string spartan6_bridge_model_from_package(const std::string &device_package)
{
	const std::string lower = to_lower_copy(device_package);
	const std::string prefix = "xc6slx";
	if (lower.rfind(prefix, 0) != 0)
		return "";

	size_t pos = prefix.size();
	while (pos < lower.size() && std::isdigit(static_cast<unsigned char>(lower[pos])))
		pos++;
	if (pos == prefix.size())
		return "";
	if (pos < lower.size() && lower[pos] == 't')
		pos++;

	return lower.substr(0, pos);
}

std::string detect_config_extension(const std::string &filename)
{
	const std::string lower = to_lower_copy(filename);
	const size_t last_dot = lower.rfind('.');
	if (last_dot == std::string::npos || last_dot + 1 >= lower.size())
		return "";

	std::string extension = lower.substr(last_dot + 1);
	if (extension == "gz" || extension == "gzip") {
		const size_t prev_dot = lower.rfind('.', last_dot - 1);
		if (prev_dot == std::string::npos || prev_dot + 1 >= last_dot)
			return extension;
		extension = lower.substr(prev_dot + 1, last_dot - prev_dot - 1);
	}

	return extension;
}

uint8_t decode_shifted_jtag_byte(const uint8_t *data, uint8_t shift)
{
	if (shift == 0)
		return McsParser::reverseByte(data[0]);
	uint8_t value = McsParser::reverseByte(data[0] >> shift);
	if (shift == 1)
		value |= (data[1] & 0x01);
	else
		value |= McsParser::reverseByte(data[1]) >> (8 - shift);
	return value;
}

void decode_shifted_jtag_stream(const uint8_t *src, uint8_t *dst,
		uint32_t len, uint8_t shift, uint32_t start_byte)
{
	for (uint32_t i = 0; i < len; i++)
		dst[i] = decode_shifted_jtag_byte(&src[start_byte + i], shift);
}

bool looks_like_valid_jedec_reply(const uint8_t *rx, uint32_t len)
{
	if (len < 3)
		return false;

	bool all_zero = true;
	bool all_ff = true;
	for (uint32_t i = 0; i < len; i++) {
		all_zero &= (rx[i] == 0x00);
		all_ff &= (rx[i] == 0xff);
	}
	if (all_zero || all_ff)
		return false;

	if ((rx[0] == 0x00 || rx[0] == 0xff) &&
			(rx[1] == 0x00 || rx[1] == 0xff) &&
			(rx[2] == 0x00 || rx[2] == 0xff))
		return false;

	return true;
}

bool looks_like_invalid_bridge_reply(const uint8_t *rx, uint32_t len)
{
	if (len == 0)
		return true;

	bool all_zero = true;
	bool all_ff = true;
	bool only_edge_artifacts = true;
	for (uint32_t i = 0; i < len; i++) {
		all_zero &= (rx[i] == 0x00);
		all_ff &= (rx[i] == 0xff);
		switch (rx[i]) {
		case 0x00:
		case 0x01:
		case 0x80:
		case 0xff:
		case 0xfe:
		case 0x7f:
			break;
		default:
			only_edge_artifacts = false;
			break;
		}
	}
	return all_zero || all_ff || only_edge_artifacts;
}

}

/* Used for xc3s */
#define USER1       0x02
#define CFG_IN      0x05
#define USERCODE    0x08
#define IDCODE      0x09
#define ISC_ENABLE  0x10
#define JPROGRAM    0x0B
#define JSTART      0x0C
#define JSHUTDOWN   0x0D
#define ISC_PROGRAM 0x11
#define ISC_DISABLE 0x16
#define BYPASS      0xff

/* xc95 instructions set */
#define XC95_IDCODE          0xfe
#define XC95_ISC_ERASE       0xed
#define XC95_ISC_ENABLE      0xe9
#define XC95_ISC_DISABLE     0xf0
#define XC95_XSC_BLANK_CHECK 0xe5
#define XC95_ISC_PROGRAM     0xea
#define XC95_ISC_READ        0xee

/* DRP instructions set */
#define XADC_DRP 0x37

/* XADC Addresses */
#define XADC_TEMP     0x00
#define XADC_LOCK     0x00
#define XADC_VCCINT   0x01
#define XADC_VCCAUX   0x02
#define XADC_VAUXEN   0x02
#define XADC_VPVN     0x03
#define XADC_RESET    0x03
#define XADC_VREFP    0x04
#define XADC_VREFN    0x05
#define XADC_VCCBRAM  0x06
#define XADC_SUPAOFFS 0x08
#define XADC_ADCAOFFS 0x09
#define XADC_ADCAGAIN 0x0a
#define XADC_VCCPINT  0x0d
#define XADC_VCCPAUX  0x0e
#define XADC_VCCODDR  0x0f
#define XADC_VAUX0    0x10
#define XADC_VAUX1    0x11
#define XADC_VAUX2    0x12
#define XADC_VAUX3    0x13
#define XADC_VAUX4    0x14
#define XADC_VAUX5    0x15
#define XADC_VAUX6    0x16
#define XADC_VAUX7    0x17
#define XADC_VAUX8    0x18
#define XADC_VAUX9    0x19
#define XADC_VAUX10   0x1a
#define XADC_VAUX11   0x1b
#define XADC_VAUX12   0x1c
#define XADC_VAUX13   0x1d
#define XADC_VAUX14   0x1e
#define XADC_VAUX15   0x1f
#define XADC_TEMP_MAX 0x20
#define XADC_TEMP_MIN 0x24
#define XADC_SUPBOFFS 0x30
#define XADC_ADCBOFFS 0x31
#define XADC_ADCBGAIN 0x32
#define XADC_FLAG     0x3f
#define XADC_CFG0     0x40
#define XADC_CFG1     0x41
#define XADC_CFG2     0x42
#define XADC_SEQ0     0x48
#define XADC_SEQ1     0x49
#define XADC_SEQ2     0x4a
#define XADC_SEQ3     0x4b
#define XADC_SEQ4     0x4c
#define XADC_SEQ5     0x4d
#define XADC_SEQ6     0x4e
#define XADC_SEQ7     0x4f
#define XADC_ALARM0   0x50
#define XADC_ALARM1   0x51
#define XADC_ALARM2   0x52
#define XADC_ALARM3   0x53
#define XADC_ALARM4   0x54
#define XADC_ALARM5   0x55
#define XADC_ALARM6   0x56
#define XADC_ALARM7   0x57
#define XADC_ALARM8   0x58
#define XADC_ALARM9   0x59
#define XADC_ALARM10  0x5a
#define XADC_ALARM11  0x5b
#define XADC_ALARM12  0x5c
#define XADC_ALARM13  0x5d
#define XADC_ALARM14  0x5e
#define XADC_ALARM15  0x5f

#define XADC_VCC_MINOFFSET 0x24
#define XADC_VCC_MAXOFFSET 0x20

/* Boundary-scan instruction set based on the FPGA model */
static std::map<std::string, std::map<std::string, std::vector<uint8_t>>>
	ircode_mapping {
		{
			/* Virtex-4 FX devices with two PowerPC blocks have a 14-bit IR.
			 * UG071: the six-bit opcodes are extended with ones in the MSBs.
			 * Bytes are stored least-significant first for JTAG shifting. */
			"virtex4_fx_dual_ppc",
			{
				{ "USER1",       {0xc2, 0x3f} },
				{ "USER2",       {0xc3, 0x3f} },
				{ "CFG_OUT",     {0xc4, 0x3f} },
				{ "CFG_IN",      {0xc5, 0x3f} },
				{ "USERCODE",    {0xc8, 0x3f} },
				{ "IDCODE",      {0xc9, 0x3f} },
				{ "ISC_ENABLE",  {0xd0, 0x3f} },
				{ "JPROGRAM",    {0xcb, 0x3f} },
				{ "JSTART",      {0xcc, 0x3f} },
				{ "JSHUTDOWN",   {0xcd, 0x3f} },
				{ "ISC_PROGRAM", {0xd1, 0x3f} },
				{ "ISC_DISABLE", {0xd7, 0x3f} },
				{ "BYPASS",      {0xff, 0x3f} },
			}
		},
		{
			/* 7-series default */
			"default",
			{
				{ "USER1",       {0x02} },
				{ "USER2",       {0x03} },
				{ "USER3",       {0x22} },
				{ "USER4",       {0x23} },
				{ "CFG_OUT",     {0x04} },
				{ "CFG_IN",      {0x05} },
				{ "USERCODE",    {0x08} },
				{ "IDCODE",      {0x09} },
				{ "ISC_ENABLE",  {0x10} },
				{ "JPROGRAM",    {0x0B} },
				{ "JSTART",      {0x0C} },
				{ "JSHUTDOWN",   {0x0D} },
				{ "ISC_PROGRAM", {0x11} },
				{ "ISC_DISABLE", {0x16} },
				{ "STATUS", 	 {0x1F} },
				{ "BYPASS",      {0xff} },
			}
		},
		{
			/* Spartan-6 USER3/USER4 differ from 7-series (UG380 Table 10-2). */
			"spartan6",
			{
				{ "USER1",       {0x02} },
				{ "USER2",       {0x03} },
				{ "USER3",       {0x1a} },
				{ "USER4",       {0x1b} },
				{ "CFG_OUT",     {0x04} },
				{ "CFG_IN",      {0x05} },
				{ "USERCODE",    {0x08} },
				{ "IDCODE",      {0x09} },
				{ "ISC_ENABLE",  {0x10} },
				{ "JPROGRAM",    {0x0B} },
				{ "JSTART",      {0x0C} },
				{ "JSHUTDOWN",   {0x0D} },
				{ "ISC_PROGRAM", {0x11} },
				{ "ISC_DISABLE", {0x16} },
				{ "BYPASS",      {0xff} },
			}
		},
		{
			/* Xilinx Virtex UltraScale+ */
			/* <vivado_dir>/data/parts/xilinx/virtexuplus/public/bsdl/xcvu9p_flga2104.bsd */
			"virtexusp",
			{
				{ "USER1",       {0b00100100, 0b00101001, 0b00} },
				{ "USER2",       {0b00100100, 0b00111001, 0b00} },
				{ "CFG_IN",      {0b00100100, 0b01011001, 0b00} },  // CFG_IN_SLR1
				{ "USERCODE",    {0b00100100, 0b10001001, 0b00} },
				{ "IDCODE",      {0b01001001, 0b10010010, 0b00} },
				{ "ISC_ENABLE",  {0b00010000, 0b00000100, 0b01} },
				{ "JPROGRAM",    {0b11001011, 0b10110010, 0b00} },
				{ "JSTART",      {0b00001100, 0b11000011, 0b00} },
				{ "JSHUTDOWN",   {0b01001101, 0b11010011, 0b00} },
				{ "ISC_PROGRAM", {0b01010001, 0b00010100, 0b01} },
				{ "ISC_DISABLE", {0b10010110, 0b01100101, 0b01} },
				{ "BYPASS",      {0b11111111, 0b11111111, 0b11} },
			}
		},
		{
			/* Xilinx Virtex UltraScale+ VU19P (xcvu19p_fsva3824) */
			/* 4-SLR SSI; SLR1 is master per BSDL */
			"virtexusp_vu19p",
			{
				{ "USER1",       {0b00100100, 0b01001001, 0b00001010} },
				{ "USER2",       {0b00100100, 0b01001001, 0b00001110} },
				{ "CFG_IN",      {0b00100100, 0b01001001, 0b00010110} },
				{ "CFG_OUT",     {0b00100100, 0b01001001, 0b00010010} },
				{ "USERCODE",    {0b00100100, 0b01001001, 0b00100010} },
				{ "IDCODE",      {0b01001001, 0b10010010, 0b00100100} },
				{ "ISC_ENABLE",  {0b00010000, 0b00000100, 0b01000001} },
				{ "JPROGRAM",    {0b11001011, 0b10110010, 0b00101100} },
				{ "JSTART",      {0b00001100, 0b11000011, 0b00110000} },
				{ "JSHUTDOWN",   {0b01001101, 0b11010011, 0b00110100} },
				{ "ISC_PROGRAM", {0b01010001, 0b00010100, 0b01000101} },
				{ "ISC_DISABLE", {0b10010110, 0b01100101, 0b01011001} },
				{ "BYPASS",      {0b11111111, 0b11111111, 0b11111111} },
			}
		}
};

/* Helper to get instruction code as a uint8_t pointer * */
static uint8_t *get_ircode(
	std::map<std::string, std::vector<uint8_t>> &inst_map, std::string inst)
{
	return inst_map.at(inst).data();
}

static void open_bitfile(
	const std::string &filename, const std::string &extension,
	ConfigBitstreamParser **parser, bool reverse, bool verbose)
{
	printInfo("Open file " + filename + " ", false);
	try {
		if (extension == "bit" || extension == "cor") {
			*parser = new BitParser(filename, reverse, verbose);
		} else if (extension == "mcs") {
			*parser = new McsParser(filename, reverse, verbose);
		} else {
			*parser = new RawParser(filename, reverse);
		}
	} catch (const std::exception &e) {
		throw std::runtime_error("Unable to open '" + filename + "': " + e.what());
	}

	printSuccess("DONE");

	printInfo("Parse file ", false);
	if ((*parser)->parse() == EXIT_FAILURE) {
		throw std::runtime_error("Failed to parse bitstream '" + filename + "'");
	}

	printSuccess("DONE");
}

#define FUSE_DNA	0x32

uint64_t Xilinx::fuse_dna_read(void)
{
	unsigned char tx_data[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	unsigned char rx_data[8];

	_jtag->go_test_logic_reset();
	_jtag->shiftIR(FUSE_DNA, 6);
	_jtag->shiftDR((unsigned char *)&tx_data, (unsigned char *)&rx_data, 64);

	uint64_t dna = 0;

	for(int i = 0; i < 8; i++) {
		unsigned char rev = 0;
		for (int j = 0; j < 8; j++) {
			rev |= ((rx_data[i] >> j) & 1) << (7 - j);
		}
		dna = (dna << 8ULL) | rev;
	}

	return dna & 0x1ffffffffffffff;
}

unsigned int Xilinx::xadc_read(uint16_t addr)
{
	unsigned int tx_data = (1 << 26) | (addr << 16);
	unsigned int rx_data = 0;

	_jtag->go_test_logic_reset();
	_jtag->shiftIR(XADC_DRP, 6);
	_jtag->shiftDR((unsigned char *)&tx_data, (unsigned char *)&rx_data, 32);
	usleep(1000);
	_jtag->shiftIR(XADC_DRP, 6);
	_jtag->shiftDR((unsigned char *)&tx_data, (unsigned char *)&rx_data, 32);

	return rx_data;
}

void Xilinx::xadc_write(uint16_t addr, uint16_t data)
{
	unsigned int tx_data = (1 << 26) | (addr << 16) | data;
	unsigned int rx_data = 0;

	_jtag->go_test_logic_reset();
	_jtag->shiftIR(XADC_DRP, 6);
	_jtag->shiftDR((unsigned char *)&tx_data, (unsigned char *)&rx_data, 32);
}

unsigned int Xilinx::xadc_single(uint16_t ch)
{
	_jtag->go_test_logic_reset();
	// single channel, disable the sequencer
	xadc_write(XADC_CFG1, 0x3000);
	// set channel, no averaging, additional settling time
	xadc_write(XADC_CFG0, (1 << 15) | (1 << 8) | ch);
	// leave some time (1ms) for the conversion
	usleep(1000);
	unsigned int ret = xadc_read(ch);

	return ret;
}

Xilinx::Xilinx(Jtag *jtag, const std::string &filename,
	const std::string &secondary_filename,
	const std::string &file_type,
	Device::prog_type_t prg_type,
	const std::string &device_package,
	const bool spi_flash_type,
	const std::string &spiOverJtagPath,
	const std::string &target_flash,
	const std::string &external_flash_type,
	bool verify, int8_t verbose,
	bool skip_load_bridge, bool skip_reset, bool read_dna, bool read_xadc,
	const std::string &dump_format):
	Device(jtag, filename, file_type, verify, verbose),
	FlashInterface(filename, verbose, 256, verify, skip_load_bridge,
			 skip_reset, external_flash_type),
	_device_package(device_package), _spiOverJtagPath(spiOverJtagPath),
	_irlen(6), _secondary_filename(secondary_filename), _soj_is_v2(false),
	_jtag_chain_len(1), _is_bpi_board(!spi_flash_type), _dump_format(dump_format)
{
	if (prg_type == Device::RD_FLASH) {
		_mode = Device::READ_MODE;
	} else if (!_file_extension.empty()) {
		if (_file_extension == "mcs") {
			_mode = Device::SPI_MODE;
		} else if (_file_extension == "bit" || _file_extension == "bin") {
			if (prg_type == Device::WR_SRAM)
				_mode = Device::MEM_MODE;
			else
				_mode = Device::SPI_MODE;
		} else if (_file_extension == "jed") {
			_mode = Device::FLASH_MODE;
		} else if (_file_extension == "pdi") {
			_mode = Device::MEM_MODE;
		}  else {
			_mode = Device::SPI_MODE;
		}
	}

	select_flash_chip(PRIMARY_FLASH);

	if (target_flash == "primary") {
		_flash_chips = PRIMARY_FLASH;
	} else if (target_flash == "secondary") {
		_flash_chips = SECONDARY_FLASH;
	} else if (target_flash == "both") {
		_flash_chips = (PRIMARY_FLASH | SECONDARY_FLASH);
	} else {
		throw std::runtime_error("Error: unknown flash target: " + target_flash);
	}

	if (_flash_chips & SECONDARY_FLASH) {
		_secondary_file_extension = secondary_filename.substr(
			secondary_filename.find_last_of(".") + 1);
		_mode = Device::SPI_MODE;
		if (!(_device_package == "xcvu9p-flga2104" || _device_package == "xcku5p-ffvb676" || _device_package == "xcku040-ffva1156")) {
			throw std::runtime_error("Error: secondary flash unavailable");
		}
	}

	uint32_t idcode = _jtag->get_target_device_id();
	std::string family = fpga_list[idcode].family;
	std::string model = fpga_list[idcode].model;
	_irlen = fpga_list[idcode].irlength;
	_ircode_map = ircode_mapping.at("default");

	if (family == "virtex4") {
		_fpga_family = VIRTEX4_FAMILY;
		_ircode_map = ircode_mapping.at("virtex4_fx_dual_ppc");
	} else if (family.substr(0, 5) == "artix") {
		_fpga_family = ARTIX_FAMILY;
	} else if (family == "spartan7") {
		_fpga_family = SPARTAN7_FAMILY;
	} else if (family == "zynq") {
		_fpga_family = ZYNQ_FAMILY;
		/* DNA read uses a 7-series PL JTAG instruction and should not
		 * be blocked by the Zynq PS-side SPI flash restriction.
		 */
		if (_mode != Device::MEM_MODE && !read_dna) {
			char mess[256];
			snprintf(mess, 256, "Error: can't flash non-volatile memory for "
				"Zynq7000 devices\n"
				"\tSPI Flash access is only available from PS side\n");
			throw std::runtime_error(mess);
		}
	} else if (family.substr(0, 6) == "zynqmp") {
		if (_mode != Device::MEM_MODE) {
			char mess[256];
			snprintf(mess, 256, "Error: can't flash non-volatile memory for "
				"ZynqMP devices\n"
				"\tSPI Flash access is only available from PSU side\n");
			throw std::runtime_error(mess);
		}
		if (!zynqmp_init(family))
			throw std::runtime_error("Error with ZynqMP init");
		_fpga_family = ZYNQMP_FAMILY;
	} else if (family == "kintex7") {
		_fpga_family = KINTEX_FAMILY;
	} else if (family == "kintexus") {
		_fpga_family = KINTEXUS_FAMILY;
	} else if (family == "kintexusp") {
		_fpga_family = KINTEXUSP_FAMILY;
	} else if (family == "artixusp") {
		_fpga_family = ARTIXUSP_FAMILY;
	} else if (family == "spartanusp") {		
		if (_file_extension != "pdi") {
			char mess[256];
			snprintf(mess, 256, "Error: only volatile PDI programing for "
				"Spartan Ultrascale+ devices\n");
			throw std::runtime_error(mess);
		}
		_fpga_family = SPARTANUSP_FAMILY;
	} else if (family == "virtexus") {
		_fpga_family = VIRTEXUS_FAMILY;
	} else if (family == "virtexusp") {
		_fpga_family = VIRTEXUSP_FAMILY;
		if (model == "xcvu19p")
			_ircode_map = ircode_mapping.at("virtexusp_vu19p");
		else
			_ircode_map = ircode_mapping.at("virtexusp");
	} else if (family.substr(0, 8) == "spartan3") {
		_fpga_family = SPARTAN3_FAMILY;
	} else if (family == "spartan6") {
		_fpga_family = SPARTAN6_FAMILY;
		_ircode_map = ircode_mapping.at("spartan6");
	} else if (family == "xcf") {
		_fpga_family = XCF_FAMILY;
		if (_mode == Device::MEM_MODE) {
			throw std::runtime_error("Error: Only write or read is supported");
		}
	} else if (family == "spartan6") {
		_fpga_family = SPARTAN6_FAMILY;
	} else if (family == "xc2c") {
		xc2c_init(idcode);
	} else if (family == "xc9500xl") {
		_fpga_family = XC95_FAMILY;
		switch (idcode) {
		case 0x09602093:
			_xc95_line_len = 2;
			break;
		case 0x09604093:
			_xc95_line_len = 4;
			break;
		case 0x09608093:
			_xc95_line_len = 8;
			break;
		case 0x09616093:
			_xc95_line_len = 16;
			break;
		}
	} else {
		_fpga_family = UNKNOWN_FAMILY;
	}

	if (read_dna) {
		if (_fpga_family == ARTIX_FAMILY || _fpga_family == KINTEXUS_FAMILY ||
			_fpga_family == ZYNQ_FAMILY) {
			uint64_t dna = Xilinx::fuse_dna_read();
			printf("{\"dna\": \"0x%016" PRIx64 "\"}\n", dna);
		} else {
			throw std::runtime_error("Error: read_dna only supported for 7-series style Xilinx FPGA");
		}
	}

	if (read_xadc) {
		if (_fpga_family == ARTIX_FAMILY || _fpga_family == KINTEXUS_FAMILY) {
			// calibrate XADC
			Xilinx::xadc_single(8);

			const int MAX_CHANNEL = 8;
			const int TEMP_MEAS   = 4;

			unsigned int v = 0;
			for (int i = 0; i < TEMP_MEAS; i++) {
				v += Xilinx::xadc_single(XADC_TEMP);
			}
			double temp = ((v/(double)TEMP_MEAS) * 503.975)/(1 << 16) - 273.15;
			double max_temp = (Xilinx::xadc_single(XADC_TEMP_MAX) * 503.975)/(1 << 16) - 273.15;
			double min_temp = (Xilinx::xadc_single(XADC_TEMP_MIN) * 503.975)/(1 << 16) - 273.15;

			unsigned int channel_values[32];
			for (int ch = 0; ch < MAX_CHANNEL; ch++) {
				if (ch < 7 || ch > 12) {
					v = Xilinx::xadc_single(ch);
				} else {
					// 7 = Invalid channel selection
					// 8 = Carry out XADC calibration
					// 9...12 = Invalid channel selection
					v = 0;
				}
				channel_values[ch] = v;
			}

			double vccint = (Xilinx::xadc_single(XADC_VCCINT)>>4)/4096.0 * 3.0; // ref: UG480
			double vccintmax = (Xilinx::xadc_single(XADC_VCCINT + XADC_VCC_MAXOFFSET)>>4)/4096.0 * 3.0; // ref: UG480
			double vccintmin = (Xilinx::xadc_single(XADC_VCCINT + XADC_VCC_MINOFFSET)>>4)/4096.0 * 3.0; // ref: UG480

			double vccaux = (Xilinx::xadc_single(XADC_VCCAUX)>>4)/4096.0 * 3.0; // ref: UG480
			double vccauxmax = (Xilinx::xadc_single(XADC_VCCAUX + XADC_VCC_MAXOFFSET)>>4)/4096.0 * 3.0; // ref: UG480
			double vccauxmin = (Xilinx::xadc_single(XADC_VCCAUX + XADC_VCC_MINOFFSET)>>4)/4096.0 * 3.0; // ref: UG480

			/* output as JSON dict */
			std::cout << "{";
			std::cout << "\"temp\": " << temp << ", " << std::endl;;
			std::cout << "    \"maxtemp\": " << max_temp << ", " << std::endl;;
			std::cout << "    \"mintemp\": " << min_temp << ", " << std::endl;;
			std::cout << "\"raw\":  {";
			for (int ch = 0; ch < MAX_CHANNEL; ch++) {
				std::cout << "\"" << ch << "\": " << channel_values[ch]
					 << ((ch == MAX_CHANNEL - 1)? "}" : ", ");
			}
			std::cout << "," << std::endl;
			std::cout << "\"vccint\": " << vccint << ", " << std::endl;
			std::cout << "   \"maxvccint\": " << vccintmax << ", " << std::endl;
			std::cout << "   \"minvccint\": " << vccintmin << ", " << std::endl;
			std::cout << "\"vccaux\": " << vccaux << ", " << std::endl;
			std::cout << "   \"maxvccaux\": " << vccauxmax << ", " << std::endl;
			std::cout << "   \"minvccaux\": " << vccauxmin << ", " << std::endl;
			std::cout << "}" << std::endl;
		} else {
			throw std::runtime_error("Error: read_xadc only supported for Artix 7");
		}
	}
}
Xilinx::~Xilinx() {}

std::string Xilinx::mode_name() const
{
	switch (_mode) {
	case Device::NONE_MODE:
		return "none";
	case Device::SPI_MODE:
		return "spi";
	case Device::MEM_MODE:
		return "mem";
	case Device::READ_MODE:
		return "read";
	}
	return "unknown";
}

std::string Xilinx::family_name() const
{
	switch (_fpga_family) {
	case XC95_FAMILY:
		return "xc95";
	case XC2C_FAMILY:
		return "xc2c";
	case VIRTEX4_FAMILY:
		return "virtex4";
	case SPARTAN3_FAMILY:
		return "spartan3";
	case SPARTAN6_FAMILY:
		return "spartan6";
	case SPARTAN7_FAMILY:
		return "spartan7";
	case ARTIX_FAMILY:
		return "artix";
	case KINTEX_FAMILY:
		return "kintex7";
	case KINTEXUS_FAMILY:
		return "kintexus";
	case KINTEXUSP_FAMILY:
		return "kintexusp";
	case ZYNQ_FAMILY:
		return "zynq";
	case ZYNQMP_FAMILY:
		return "zynqmp";
	case XCF_FAMILY:
		return "xcf";
	case ARTIXUSP_FAMILY:
		return "artixusp";
	case SPARTANUSP_FAMILY:
		return "spartanusp";
	case VIRTEXUS_FAMILY:
		return "virtexus";
	case VIRTEXUSP_FAMILY:
		return "virtexusp";
	case UNKNOWN_FAMILY:
		return "unknown";
	}
	return "unknown";
}

std::string Xilinx::flash_target_name() const
{
	if (_flash_chips == PRIMARY_FLASH)
		return "primary";
	if (_flash_chips == SECONDARY_FLASH)
		return "secondary";
	if (_flash_chips == (PRIMARY_FLASH | SECONDARY_FLASH))
		return "both";
	return "mask=" + std::to_string(_flash_chips);
}

std::string Xilinx::debug_context() const
{
	std::ostringstream oss;
	oss << "file=" << (_filename.empty() ? "<none>" : _filename)
		<< ", secondary_file="
		<< (_secondary_filename.empty() ? "<none>" : _secondary_filename)
		<< ", extension=" << (_file_extension.empty() ? "<auto>" : _file_extension)
		<< ", secondary_extension="
		<< (_secondary_file_extension.empty() ? "<none>" : _secondary_file_extension)
		<< ", mode=" << mode_name()
		<< ", family=" << family_name()
		<< ", device_package="
		<< (_device_package.empty() ? "<none>" : _device_package)
		<< ", flash_target=" << flash_target_name()
		<< ", user_instruction="
		<< (_user_instruction.empty() ? "<none>" : _user_instruction)
		<< ", verify=" << yes_no(_verify)
		<< ", skip_load_bridge=" << yes_no(_skip_load_bridge)
		<< ", skip_reset=" << yes_no(_skip_reset)
		<< ", bridge_path="
		<< (_spiOverJtagPath.empty() ? "<auto>" : _spiOverJtagPath)
		<< ", bpi_board=" << yes_no(_is_bpi_board);
	return oss.str();
}

bool Xilinx::zynqmp_init(const std::string &family)
{
	/* by default, at powering a zynqmp has
	 * PL TAP and ARM DAP disabled
	 * at this time only PS TAB and a dummy are seen
	 * So first step is to enable PL and ARM
	 */
	if (family == "zynqmp_cfgn") {
		/* PS TAP is the first device with 0xfffffe idcode */
		_jtag->device_select(0);
		/* send 0x03 into JTAG_CTRL register */
		uint16_t ircode = 0x824;
		_jtag->shiftIR(ircode & 0xff, 8, Jtag::SHIFT_IR);
		_jtag->shiftIR((ircode >> 8) & 0x0f, 4);
		uint8_t instr[4] = {0x3, 0, 0, 0};
		_jtag->shiftDR(instr, NULL, 32);
		/* synchronize everything by moving to TLR */
		_jtag->set_state(Jtag::TEST_LOGIC_RESET);
		_jtag->toggleClk(10);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(100);
		/* force again JTAG chain detection */
		_jtag->detectChain(5);
	}
	/* check if the chain is correctly configured:
	 * 2 devices
	 * PL at position 0
	 * ARM at position 1
	 */
	char mess[256];
	std::vector<uint32_t> listDev = _jtag->get_devices_list();
	if (listDev.size() != 2) {
		snprintf(mess, sizeof(mess), "ZynqMP error: wrong"
				" JTAG length: %zu instead of 2\n",
				listDev.size());
		printError(mess);
		if (!listDev.empty()) {
			std::ostringstream oss;
			oss << "ZynqMP detected chain: ";
			for (size_t i = 0; i < listDev.size(); ++i) {
				if (i != 0)
					oss << ", ";
				oss << "[" << i << "]=" << format_hex32(listDev[i]);
			}
			printError(oss.str());
		}
		return false;
	}

	if (fpga_list[listDev[0]].family != "zynqmp") {
		snprintf(mess, sizeof(mess), "ZynqMP error: first device"
				" is not the PL TAP -> 0x%08x\n",
				listDev[0]);
		printError(mess);
		printError("ZynqMP init context: " + debug_context());
		return false;
	}

	if (listDev[1] != 0x5ba00477) {
		snprintf(mess, sizeof(mess), "ZynqMP error: second device"
				" is not the ARM DAP cortex A53 -> 0x%08x\n",
				listDev[1]);
		printError(mess);
		printError("ZynqMP init context: " + debug_context());
		return false;
	}

	_jtag->insert_first(0xdeadbeef, 6);
	_jtag->device_select(1);
	_irlen = 6;

	return true;
}

void Xilinx::reset()
{
	_jtag->shiftIR(get_ircode(_ircode_map, "JSHUTDOWN"), NULL, _irlen);
	_jtag->shiftIR(get_ircode(_ircode_map, "JPROGRAM"), NULL, _irlen);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(10000*12);

	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);

	_jtag->shiftIR(get_ircode(_ircode_map, "BYPASS"), NULL, _irlen);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(2000);
}

uint32_t Xilinx::idCode()
{
	int id = 0;
	unsigned char tx_data[4]= {0x00, 0x00, 0x00, 0x00};
	unsigned char rx_data[4];
	_jtag->go_test_logic_reset();

	_jtag->shiftIR(get_ircode(_ircode_map, "IDCODE"), NULL, _irlen);
	_jtag->shiftDR(tx_data, rx_data, 32);
	id = ((rx_data[0] & 0x000000ff) |
		((rx_data[1] << 8) & 0x0000ff00) |
		((rx_data[2] << 16) & 0x00ff0000) |
		((rx_data[3] << 24) & 0xff000000));

	/* workaround for XC95 with different
	 * IR length and IDCODE value
	 */
	if (id == 0) {
		_jtag->go_test_logic_reset();
		_jtag->shiftIR(XC95_IDCODE, 8);
		_jtag->shiftDR(tx_data, rx_data, 32);
		id = ((rx_data[0] & 0x000000ff) |
			((rx_data[1] << 8) & 0x0000ff00) |
			((rx_data[2] << 16) & 0x00ff0000) |
			((rx_data[3] << 24) & 0xff000000));
	}

	return id;
}

void Xilinx::program(unsigned int offset, bool unprotect_flash)
{
	ConfigBitstreamParser *bit = nullptr;
	ConfigBitstreamParser *secondary_bit = nullptr;
	bool reverse = false;

	/* nothing to do */
	if (_mode == Device::NONE_MODE || _mode == Device::READ_MODE)
		return;

	if (_mode == Device::FLASH_MODE && _file_extension == "jed") {
		if (_fpga_family != XC95_FAMILY && _fpga_family != XC2C_FAMILY)
			throw std::runtime_error("Error: jed only supported for xc95 and xc2c");
		printInfo("Open file ", false);

		std::unique_ptr<JedParser> jed(new JedParser(_filename, _verbose));
		if (jed->parse() == EXIT_FAILURE) {
			printError("FAIL");
			return;
		}
		printSuccess("DONE");

		if (_fpga_family == XC95_FAMILY)
			flow_program(jed.get());
		else if (_fpga_family == XC2C_FAMILY)
			xc2c_flow_program(jed.get());
		return;
	}

	if (_fpga_family == XC95_FAMILY) {
		printError("Only jed file and flash mode supported for XC95 CPLD");
		return;
	}

	if (_mode == Device::MEM_MODE || _fpga_family == XCF_FAMILY)
		reverse = true;

	if (_file_extension == "pdi")
		reverse = false;

	try {
		if (_flash_chips & PRIMARY_FLASH) {
			open_bitfile(_filename, _file_extension, &bit, reverse, _verbose);
		}
		if (_flash_chips & SECONDARY_FLASH) {
			open_bitfile(_secondary_filename, _secondary_file_extension,
				&secondary_bit, reverse, _verbose);
		}
	} catch (std::exception &e) {
		printError("Xilinx bitstream open context: " + debug_context());
		if (bit)
			delete bit;
		if (secondary_bit)
			delete secondary_bit;
		throw std::runtime_error(e.what());
	}

	if (_verbose) {
		if (bit)
			bit->displayHeader();
		if (secondary_bit)
			secondary_bit->displayHeader();
	}

	if (_fpga_family == XCF_FAMILY) {
		xcf_program(bit);
		delete bit;
		return;
	}

	if (_mode == Device::SPI_MODE) {
		/* Check for BPI flash boards */
		if (_is_bpi_board) {
			printInfo("Programming via BPI bridge");
			program_bpi(bit, offset);
		} else {
			if (_flash_chips & PRIMARY_FLASH) {
				select_flash_chip(PRIMARY_FLASH);
				if (_verbose)
					printInfo("Programming primary SPI flash with " + debug_context());
				program_spi(bit, _file_extension, offset, unprotect_flash);
			}
			if (_flash_chips & SECONDARY_FLASH) {
				select_flash_chip(SECONDARY_FLASH);
				if (_verbose)
					printInfo("Programming secondary SPI flash with " + debug_context());
				program_spi(secondary_bit, _secondary_file_extension, offset, unprotect_flash);
			}
		}
	} else {
		if (_fpga_family == SPARTAN3_FAMILY)
			xc3s_flow_program(bit);
		else
			program_mem(bit);
	}

	delete bit;
}

bool Xilinx::post_flash_access()
{
	if (_skip_reset)
		printInfo("Skip resetting device");
	else
		reset();
	return true;
}

bool Xilinx::prepare_flash_access()
{
	bool ret = false;
	if (_skip_load_bridge) {
		printInfo("Skip loading bridge for spiOverjtag");
		ret = true;
	} else {
		if (_verbose)
			printInfo("Preparing flash access with " + debug_context());
		ret = load_bridge();
	}

	/* Get number of FPGAs in the Jtag Chain */
	_jtag_chain_len = _jtag->get_chain_len();

	/* Keep the cable in its current JTAG transport mode here.
	 * Some XPCU variants detect and access Spartan-6 more reliably through
	 * the accelerated path, and the low-level implementation already falls
	 * back to control-transfer bitbang if the bulk engine cannot service a
	 * scan. */
	if (_fpga_family == SPARTAN6_FAMILY) {
		if (auto *ll = _jtag->get_ll_class(); ll != nullptr) {
			ll->setUserScanCaptureMode(true);
		}
	}

	/* check SpiOverJtag version */
	if (ret) {
		if (get_spiOverJtag_version() == 2.0f)
			_soj_is_v2 = true;
		printf("SOJ version: %f\n", _soj_is_v2 ? 2.0f : 1.0f);
	}
	return ret;
}

bool Xilinx::load_bridge()
{
	std::string bitname;
	std::string extension;
	if (!_spiOverJtagPath.empty()) {
		bitname = _spiOverJtagPath;
		extension = detect_config_extension(bitname);
	} else {
		if (_device_package.empty()) {
			printError("Can't program SPI flash: missing device-package information");
			printError("SPI-over-JTAG requires a bridge bitstream specific to your FPGA package.");
			printError("Provide it with --fpga-part (e.g. --fpga-part xc7a200tfbg484) or --board <boardname>.");
			printError("SPI bridge context: " + debug_context());
			return false;
		}

		const std::string bridge_dir = get_shell_env_var("OPENFPGALOADER_SOJ_DIR",
			DATA_DIR "/openFPGALoader");
		if (_fpga_family == SPARTAN6_FAMILY) {
			const std::string model = spartan6_bridge_model_from_package(
				_device_package);
			if (!model.empty()) {
				const std::string cor_path = bridge_dir +
					"/from_ise/spartan-6/" + model + "_spi.cor";
				if (file_exists(cor_path)) {
					bitname = cor_path;
					extension = "cor";
				}
			}
		}
		if (bitname.empty()) {
			bitname = bridge_dir + "/spiOverJtag_" + _device_package +
				".bit.gz";
			extension = "bit";
		}
	}

#if defined (_WIN64) || defined (_WIN32)
	/* Convert relative path embedded at compile time to an absolute path */
	bitname = PathHelper::absolutePath(bitname);
#endif

	/* first: load spi over jtag */
	ConfigBitstreamParser *bridge = nullptr;
	try {
		open_bitfile(bitname, extension, &bridge, true, _verbose);
		printSuccess("Use: " + bridge->getFilename());
		if (_verbose && !_spiOverJtagPath.empty()) {
			printInfo("SPI bridge parser: explicit file extension '" +
				(extension.empty() ? std::string("<raw>") : extension) + "'");
		}
		if (_fpga_family == SPARTAN3_FAMILY)
			xc3s_flow_program(bridge);
		else
			program_mem(bridge);
	} catch (std::exception &e) {
		delete bridge;
		printError("SPI bridge load context: " + debug_context() +
			", resolved_bridge=" + bitname +
			", bridge_extension=" + (extension.empty() ? std::string("<raw>") : extension));
		printError(e.what());
		throw std::runtime_error(e.what());
	}
	delete bridge;

	return true;
}

bool Xilinx::load_bpi_bridge()
{
	std::string bitname;
	std::string extension = "bit";

	if (_device_package.empty()) {
		printError("Can't program BPI flash: missing device-package information");
		printError("BPI-over-JTAG requires a bridge bitstream specific to your FPGA package.");
		printError("Provide it with --fpga-part (e.g. --fpga-part xc7a200tfbg484) or --board <boardname>.");
		printError("BPI bridge context: " + debug_context());
		return false;
	}

	bitname = get_shell_env_var("OPENFPGALOADER_SOJ_DIR", DATA_DIR "/openFPGALoader");
	bitname += "/bpiOverJtag_" + _device_package + ".bit.gz";

#if defined (_WIN64) || defined (_WIN32)
	bitname = PathHelper::absolutePath(bitname);
#endif

	/* Load BPI over JTAG bridge */
	ConfigBitstreamParser *bridge = nullptr;
	try {
		open_bitfile(bitname, extension, &bridge, true, _verbose);
		printSuccess("Use: " + bridge->getFilename());
		program_mem(bridge);
	} catch (std::exception &e) {
		delete bridge;
		printError("BPI bridge load context: " + debug_context() +
			", resolved_bridge=" + bitname +
			", bridge_extension=" + extension);
		printError(e.what());
		throw std::runtime_error(e.what());
	}
	delete bridge;

	/* Initialize BPI flash instance */
	_bpi_flash.reset(new BPIFlash(_jtag, _verbose));

	return true;
}

void Xilinx::program_bpi(ConfigBitstreamParser *bit, unsigned int offset)
{
	if (!bit)
		throw std::runtime_error("called with null bitstream");
	if (_verbose)
		printInfo("BPI programming context: " + debug_context() +
			", offset=" + std::to_string(offset));

	/* Load BPI bridge if not already loaded */
	if (!_bpi_flash) {
		if (!load_bpi_bridge()) {
			throw std::runtime_error("Failed to load BPI bridge");
		}
	}

	/* Detect flash */
	if (!_bpi_flash->detect()) {
		printError("BPI flash detect context: " + debug_context());
		throw std::runtime_error("BPI flash detection failed");
	}

	/* Program the flash */
	const uint8_t *data = bit->getData();
	int length = bit->getLength() / 8;

	if (!_bpi_flash->write(offset, data, length)) {
		throw std::runtime_error("BPI flash programming failed");
	}

	/* Reset the board if skip_reset is not set */
	post_flash_access();

	printInfo("BPI flash programming complete");
}

bool Xilinx::dumpFlash_bpi(uint32_t base_addr, uint32_t len)
{
	if (!_bpi_flash) {
		if (!load_bpi_bridge())
			return false;
	}
	if (!_bpi_flash->detect()) {
		printError("BPI flash detection failed");
		return false;
	}
	if (len == 0)
		len = _bpi_flash->capacity();
	std::vector<uint8_t> buf(len);
	printInfo("Reading BPI flash (slow, one word per USB round-trip)...");
	if (!_bpi_flash->read(buf.data(), base_addr, len)) {
		printError("BPI Flash read failed");
		return false;
	}
	FILE *fd = fopen(_filename.c_str(), "wb");
	if (!fd) {
		printError("Can't open dump file");
		return false;
	}
	fwrite(buf.data(), 1, len, fd);
	fclose(fd);
	printSuccess("BPI dump DONE");
	return post_flash_access();
}

float Xilinx::get_spiOverJtag_version()
{
	uint8_t jtx[6] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
	uint8_t jrx[7];
	uint8_t rx[6];
	const uint8_t shift = _jtag_chain_len;

	auto probe_version = [&]() {
		_jtag->shiftIR(get_ircode(_ircode_map, "USER4"), NULL, _irlen,
			Jtag::UPDATE_IR);
		if (_verbose > 0)
			printf("jtag_chain_len: %d\n", _jtag->get_chain_len());
		if (_jtag_chain_len > 1)
			_jtag->shiftDR(jtx, NULL, _jtag_chain_len - 1, Jtag::SHIFT_DR);
		_jtag->shiftDR(jtx, jrx, 6 * 8);
		_jtag->flush();

		decode_shifted_jtag_stream(jrx, rx, 5, shift, 1);
		if (_fpga_family == SPARTAN6_FAMILY &&
				looks_like_invalid_bridge_reply(rx, 5))
			decode_shifted_jtag_stream(jrx, rx, 5, shift, 0);
		rx[5] = '\0';
	};

	probe_version();

	if (_fpga_family == SPARTAN6_FAMILY &&
			looks_like_invalid_bridge_reply(rx, 5)) {
		if (auto *ll = _jtag->get_ll_class();
				ll != nullptr && ll->selectAlternateTdoMask()) {
			printWarn("Retrying Spartan-6 spiOverJtag version probe with alternate XPCU TDO mask");
			probe_version();
			ll->restorePrimaryTdoMask();
		}
	}

	/* Clear stale USER4 scan chain data from the bridge shift register.
	 * Without this TLR reset, the bridge retains the version query response
	 * bytes when USER1 is selected for SPI operations, corrupting the first
	 * SPI transfer (e.g. RDID returns garbage like 0x1898E8 instead of a
	 * valid JEDEC ID). */
	_jtag->go_test_logic_reset();

	/*
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
			       "has v2 start bit set\n",
			       jrx[0], rev0);
		}

		/* Try version query in SOJ v2 format to check for genuine v2 bridge */
		if (_verbose > 0) {
			printf("Trying SOJ v2 version query...\n");
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
			printf("\n");
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
			printf("\nSOJ v2 version string has %d data bytes — assuming v2\n",
			       v2_nonzero_noncmd);
			return 2.0f;
		}

		if ((rev0 & 0x01) && (_verbose > 0))
			printf("Raw v2 header bit set, but no version string data — staying v1\n");
	}

	if (_verbose > 0) {
		printf("SOJ version raw:");
		for (size_t i = 0; i < sizeof(jrx); i++)
			printf(" %02x", jrx[i]);
		printf("\nSOJ version decoded:");
		for (int i = 0; i < 5; i++)
			printf(" %02x", rx[i]);
		printf(" -> '%s'\n", rx);
	}

	/* Check raw jrx for plain ASCII version string "0X.XX" pattern.
	 * Some SOJ bridges (e.g. Digilent HS3 with Artix-7) return the version
	 * as plain ASCII in the jrx buffer (e.g. 00 30 32 2e 30 30 00 = "02.00")
	 * even though decode_shifted_jtag_stream mangles it. This check applies
	 * to ALL families — not just UltraScale+ — since custom bridge bitstreams
	 * may use any family. */
	for (size_t off = 1; off + 5 < sizeof(jrx); off++) {
		if (jrx[off] >= '0' && jrx[off] <= '9' &&
		    jrx[off+1] >= '0' && jrx[off+1] <= '9' &&
		    jrx[off+2] == '.' &&
		    jrx[off+3] >= '0' && jrx[off+3] <= '9' &&
		    jrx[off+4] >= '0' && jrx[off+4] <= '9' &&
		    jrx[off+5] == '\0') {
			char asciiVer[6] = {0};
			memcpy(asciiVer, &jrx[off], 5);
			if (_verbose > 0)
				printf("SOJ version from raw ASCII: '%s'\n", asciiVer);
			float asciiVerNum = atof(asciiVer);
			if (asciiVerNum >= 2.0f) {
				if (_verbose > 0)
					printf("Detected SOJ v2 from raw ASCII version string\n");
				return 2.0f;
			}
			break;
		}
	}

	float version = atof((const char *)rx);
	if (version == 0.0f)  // not supported => 1.0
		return 1.0f;
	return version;
}

void Xilinx::program_spi(ConfigBitstreamParser * bit, std::string extention,
		unsigned int offset, bool unprotect_flash)
{
	if (!bit)
		throw std::runtime_error("called with null bitstream");
	if (extention == "mcs") {
		McsParser *parser = (McsParser *)bit;
		if (!FlashInterface::write(parser->getRecords(), unprotect_flash, true))
			throw std::runtime_error("SPI flash write failed");
	} else {
		const uint8_t *data = bit->getData();
		int length = bit->getLength() / 8;
		if (!FlashInterface::write(offset, data, length, unprotect_flash))
			throw std::runtime_error("SPI flash write failed");
	}
}

void Xilinx::program_mem(ConfigBitstreamParser *bitfile)
{
	std::cout << "load program" << std::endl;
	unsigned char *tx_buf;
	unsigned char rx_buf[(_irlen >> 3) + 1];

	/*            comment                                TDI   TMS TCK
	 * 1: On power-up, place a logic 1 on the TMS,
	 *    and clock the TCK five times. This ensures      X     1   5
	 *    starting in the TLR (Test-Logic-Reset) state.
	 */
	_jtag->go_test_logic_reset();
	/*
	 * 2: Move into the RTI state.                        X     0   1
	 * 3: Move into the SELECT-IR state.                  X     1   2
	 * 4: Enter the SHIFT-IR state.                       X     0   2
	 * 5: Start loading the JPROGRAM instruction,     01011(4)  0   5
	 *    LSB first:
	 * 6: Load the MSB of the JPROGRAM instruction
	 *    when exiting SHIFT-IR, as defined in the        0     1   1
	 *    IEEE standard.
	 * 7: Place a logic 1 on the TMS and clock the
	 *    TCK five times. This ensures starting in        X     1   5
	 *    the TLR (Test-Logic-Reset) state.
	 */
	_jtag->shiftIR(get_ircode(_ircode_map, "JPROGRAM"), NULL, _irlen);
	/* Poll INIT_B (bit 4 of IR capture) until config memory is cleared */
	tx_buf = get_ircode(_ircode_map, "BYPASS");
	do {
		_jtag->shiftIR(tx_buf, rx_buf, _irlen);
	} while (!(rx_buf[0] & 0x10));
	/*
	 * 8: Move into the RTI state.                        X     0   10,000(1)
	 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(10000*12);
	/*
	 * 9: Start loading the CFG_IN instruction,
	 *    LSB first:                                    00101   0   5
	 * 10: Load the MSB of CFG_IN instruction when
	 *     exiting SHIFT-IR, as defined in the            0     1   1
	 *     IEEE standard.
	 */
	_jtag->shiftIR(get_ircode(_ircode_map, "CFG_IN"), NULL, _irlen);
	/*
	 * 11: Enter the SELECT-DR state.                     X     1   2
	 */
	_jtag->set_state(Jtag::SELECT_DR_SCAN);
	/*
	 * 13: Shift in the FPGA bitstream. Bitn (MSB)
	 *     is the first bit in the bitstream(2).    bit1...bitn 0  (bits in bitstream)-1
	 * 14: Shift in the last bit of the bitstream.
	 *     Bit0 (LSB) shifts on the transition to       bit0    1   1
	 *     EXIT1-DR.
	 */
	/* GGM: TODO */
	int byte_length = bitfile->getLength() / 8;
	const uint8_t *data = bitfile->getData();
	int tx_len;
	Jtag::tapState_t tx_end;
	const int burst_len = (byte_length < 100) ? byte_length : byte_length / 100;

	ProgressBar progress("Load SRAM", byte_length, 50, _quiet);

	for (int i=0; i < byte_length; i+=burst_len) {
		if (i + burst_len > byte_length) {
			tx_len = (byte_length - i) * 8;
			/*
			 * 15: Enter UPDATE-DR state.                 X     1   1
			 */
			tx_end = Jtag::UPDATE_DR;
		} else {
			tx_len = burst_len * 8;
			/*
			 * 12: Enter the SHIFT-DR state.              X     0   2
			 */
			tx_end = Jtag::SHIFT_DR;
		}
		_jtag->shiftDR(data+i, NULL, tx_len, tx_end);
		_jtag->flush();
		progress.display(i);
	}
	progress.done();
	/*
	 * 16: Move into RTI state.                           X     0   1
	 */
	_jtag->set_state(Jtag::RUN_TEST_IDLE);

	if (_file_extension == "pdi") {
		_jtag->toggleClk(2000);
		/*
		* 17: For PDI devices, use the STATUS instruction 
		*     to verify successful configuration.          
		*/
		unsigned char tx_data[6]= {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
		unsigned char rx_data[6];
		_jtag->shiftIR(get_ircode(_ircode_map, "STATUS"), NULL, _irlen);
		_jtag->shiftDR(tx_data, rx_data, 48);
		if ((rx_data[4] & 0x04) != 0x04) {
				printError("PDI programing failed");
			} else {
				printSuccess("PDI programing success");
			}
		}
	else {
		/*
		* 17: Enter the SELECT-IR state.                     X     1   2
		* 18: Move to the SHIFT-IR state.                    X     0   2
		* 19: Start loading the JSTART instruction
		*     (optional). The JSTART instruction           01100   0   5
		*     initializes the startup sequence.
		* 20: Load the last bit of the JSTART instruction.   0     1   1
		* 21: Move to the UPDATE-IR state.                   X     1   1
		*/
		_jtag->shiftIR(get_ircode(_ircode_map, "JSTART"), NULL, _irlen, Jtag::UPDATE_IR);
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
		printf("Shift IR %02x\n", rx_buf[0]);
		printf("ir: %x isc_done %x isc_ena %x init %x done %x\n", ir_c, isc_done, isc_ena,
			init, done);

		if (!done) {
			read_register("STAT");
		}
	}
}

static uint32_t char_array_to_word(uint8_t *in)
{
	return (((uint32_t)in[3] << 24) |
		((uint32_t)in[2] << 16) |
		((uint32_t)in[1] <<  8) |
		((uint32_t)in[0] <<  0));
}

typedef struct {
	std::string description;
	uint8_t offset;
	uint8_t size;
	std::map<int, std::string> reg_cnt;
} reg_struct_t;

#define REG_ENTRY(_description, _offset, _size, ...) \
	{_description, _offset, _size, {__VA_ARGS__}}

static const std::map<std::string, std::list<reg_struct_t>> reg_content = {
	{"CTRL0", std::list<reg_struct_t>{
		REG_ENTRY("GTS USR B",       0, 1,
			{0, "I/Os 3-stated"}, {1, "I/Os active"}),
		REG_ENTRY("Reserved",        1, 2),
		REG_ENTRY("PERSIST",         3, 1,
			{0, "No"}, {1, "Yes"}),
		REG_ENTRY("SBITS",           4, 2,
			{0, "Read/Write OK"}, {1, "Readback disabled"},
			{2, "Both Writes and Reads disabled"},
			{3, "Both Writes and Reads disabled"}),
		REG_ENTRY("DEC",             6, 1,
			{0, "Disable"}, {1, "Enable"}),
		REG_ENTRY("FARSRC",          7, 1),
		REG_ENTRY("GLUMASK B",       8, 1),
		REG_ENTRY("Reserved",        9, 1),
		REG_ENTRY("ConfigFallback", 10, 1,
			{0, "Disable"}, {1, "Enable"}),
		REG_ENTRY("Reserved",       11, 1),
		REG_ENTRY("OverTempPwrDown", 12, 1,
			{0, "Disable"}, {1, "Enable"}),
		REG_ENTRY("Reserved",       13, 17),
		REG_ENTRY("ICAP Select",    30, 1,
			{0, "Top ICAPE2 Port Enabled"},
			{1, "Bottom ICAPE2 Port Enabled"}),
		REG_ENTRY("EFUSE key",      31, 1,
			{0, "Battery backed RAM"}, {1, "eFUSE"}),
	}},
	{"STAT", std::list<reg_struct_t>{
		REG_ENTRY("CRC Error",       0, 1,
			{0, "No CRC error"}, {1, "CRC error"}),
		REG_ENTRY("Part Secured",    1, 1),
		REG_ENTRY("MMCM lock",       2, 1),
		REG_ENTRY("DCI match",       3, 1),
		REG_ENTRY("EOS",             4, 1),
		REG_ENTRY("GTS CFG B",       5, 1),
		REG_ENTRY("GWE",             6, 1),
		REG_ENTRY("GHIGH B",         7, 1),
		REG_ENTRY("MODE",            8, 3),
		REG_ENTRY("INIT Complete",  11, 1),
		REG_ENTRY("INIT B",         12, 1),
		REG_ENTRY("Release Done",   13, 1),
		REG_ENTRY("Done",           14, 1),
		REG_ENTRY("ID Error",       15, 1,
			{0, "No ID error"}, {1, "ID error"}),
		REG_ENTRY("DEC Error",      16, 1),
		REG_ENTRY("XADC Over temp", 17, 1),
		REG_ENTRY("STARTUP State",  18, 3),
		REG_ENTRY("Reserved",       21, 4),
		REG_ENTRY("BUS Width",      25, 2,
			{0, "x1"}, {1, "x8"}, {2, "x16"}, {3, "x32"}),
		REG_ENTRY("Reserved",       27, 5)
	}},
	// UG470 Table 5-34
	{"WBSTAR", std::list<reg_struct_t>{
		// Next bitstream start address. The default start address
		//   is address zero.
		REG_ENTRY("START ADDR",  0, 29),
		REG_ENTRY("RS TS B",    29, 1,
			{0, "3-state enabled (RS[1:0] disabled) (default)"},
			{1, "3-state disabled (RS[1:0] enabled)"},
		),
		// RS[1:0] pin value on next warm boot. The default is 00.
		REG_ENTRY("RS[1:0]",    30, 2),
	}},
	// UG470 Table 5-39
	{"BOOTSTS", std::list<reg_struct_t>{
		// Status 0 is valid
		REG_ENTRY("VALID 0",       0, 1),
		REG_ENTRY("FALLBACK 0",    1, 1,
			{0, "Normal configuration"},
			{1, "Fallback to default reconfiguration, RS[1:0] actively drives 2'b00"}
		),
		// Internal PROG triggered configuration
		REG_ENTRY("IPROG 0",       2, 1),
		// Watchdog time-out error
		REG_ENTRY("WTO Error 0",   3, 1),
		// ID_CODE error
		REG_ENTRY("ID Error 0",    4, 1),
		// CRC error
		REG_ENTRY("CRC Error 0",   5, 1),
		// BPI address counter wraparound error, supported in
		//   asynchronous read mode
		REG_ENTRY("WRAP Error 0",  6, 1),
		// HMAC error
		REG_ENTRY("HMAC Error 0",  7, 1),
		REG_ENTRY("VALID 1",       8, 1),
		REG_ENTRY("FALLBACK 1",    9, 1,
			{0, "Normal configuration"},
			{1, "Fallback to default reconfiguration, RS[1:0] actively drives 2'b00"}
		),
		REG_ENTRY("IPROG 1",      10, 1),
		REG_ENTRY("WTO Error 1",  11, 1),
		REG_ENTRY("ID Error 1",   12, 1),
		REG_ENTRY("CRC Error 1",  13, 1),
		REG_ENTRY("WRAP Error 1", 14, 1),
		REG_ENTRY("HMAC Error 1", 15, 1),
	}},
};

/* UG470 table 5-23 */
static const std::map<std::string, uint8_t> reg_code = {
	{"CTRL0",   0x05},  // Control register 0
	{"STAT",    0x07},  // Status register
	{"CONF0",   0x09},  // Configuration Option 0
	{"CONF1",   0x0e},  // Configuration Option 1
	{"WBSTAR",  0x10},  // Warm Boot Start Address Register
	{"BOOTSTS", 0x16},  // Boot history status register
	{"CTRL1",   0x18},  // Control register 1
};

uint32_t Xilinx::dumpRegister(std::string reg_name)
{
	uint8_t reg[4];
	const uint8_t dummy[] = {0x00, 0x00, 0x00, 0x04};
	/* opcode:
	 * 0x0: NOP
	 * 0x1: read
	 * 0x2: write
	 * 0x3: Reserved
	 */
	/* register code
	 * 0x05: Control register 0
	 * 0x07: Status register
	 * 0x09: Configuration Option 0
	 * 0x0e: Configuration Option 1
	 * 0x18: Control Register 1
	 * 0x16: Boot history Status register
	 */
	auto code = reg_code.find(reg_name);
	if (code == reg_code.end()) {
		printError("Unknown register " + reg_name);
		printError("Known Register are:");
		for (auto reg : reg_code)
			printError("\t" + reg.first);
		return 0xdeadbeef;
	}

	/* packet type 1 */
	const uint32_t regcode = static_cast<uint32_t>(code->second);
	uint32_t cfg_packets[] = {
		0xAA995566,  // Sync Word
		0x20000000,  // NOOP
		((0x01    & 0x0007) << 29) |  // [31:29]: Header type
		((0x01    & 0x0003) << 27) |  // [28:27]: opcode
		((regcode & 0x3fff) << 13) |  // [26:13]: register code
		((0x00    & 0x0003) << 11) |  // [12:11]: Reserved must be set to 0
		((0x01    & 0x07ff) <<  0),   // [10:0 ]: word count
		0x20000000,  // NOOP
		0x20000000,  // NOOP
	};

	_jtag->go_test_logic_reset();
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->shiftIR(get_ircode(_ircode_map, "CFG_IN"), NULL, _irlen, Jtag::SELECT_DR_SCAN);

	Jtag::tapState_t next_state = Jtag::SHIFT_DR;
	for (int i = 0; i < 5; i++) {
		if (i == 4)
			next_state = Jtag::SELECT_IR_SCAN;
		const uint32_t tmp = BitParser::reverse_32(cfg_packets[i]);
		const uint8_t cfg[] = {
			static_cast<uint8_t>((tmp >>  0) & 0xff),
			static_cast<uint8_t>((tmp >>  8) & 0xff),
			static_cast<uint8_t>((tmp >> 16) & 0xff),
			static_cast<uint8_t>((tmp >> 24) & 0xff)
		};

		_jtag->shiftDR(cfg, NULL, 32, next_state);
	}

	_jtag->shiftIR(get_ircode(_ircode_map, "CFG_OUT"), NULL, _irlen, Jtag::SELECT_DR_SCAN);
	_jtag->shiftDR(dummy, reg, 32);
	_jtag->go_test_logic_reset();

	uint32_t reg_word = char_array_to_word(reg);
	reg_word = BitParser::reverse_32(reg_word);
	return reg_word;
}

void Xilinx::displayRegister(const std::string reg_name, const uint32_t reg_val) {
	auto reg = reg_content.find(reg_name);
	if (reg == reg_content.end()) {
		printError("Unknown register " + reg_name);
		return;
	}

	std::stringstream raw_val;
	raw_val << "0x" << std::hex << reg_val;
	printSuccess("Register raw value: " + raw_val.str());

	const std::list<reg_struct_t> regs = reg->second;
	for (reg_struct_t r: regs) {
		uint8_t offset = r.offset;
		uint8_t size = r.size;
		uint32_t mask = (1 << size) - 1;
		uint32_t val = (reg_val >> offset) & mask;
		std::stringstream ss, desc;
		desc << r.description;
		ss << std::setw(16) << std::left << r.description;
		if (r.reg_cnt.size() != 0) {
			ss << r.reg_cnt[val];
		} else {
			std::stringstream hex_val;
			hex_val << "0x" << std::hex << val;
			ss << hex_val.str();
		}

		printInfo(ss.str());
	}
}

bool Xilinx::dumpFlash(uint32_t base_addr, uint32_t len)
{
	/* BPI parallel NOR dump (added: upstream dumpFlash lacks a BPI path) */
	if (_is_bpi_board)
		return dumpFlash_bpi(base_addr, len);

	if (_fpga_family == XC95_FAMILY || _fpga_family == XCF_FAMILY) {
		std::string buffer;
		if (_fpga_family == XC95_FAMILY) {
			/* enable ISC */
			flow_enable();
			buffer = flow_read();
			/* disable ISC */
			flow_disable();
		} else {
			/* enable ISC */
			xcf_flow_enable(0x34);
			buffer = xcf_read();
			/* disable ISC */
			xcf_flow_disable();
		}
		printInfo("Open dump file ", false);
		FILE *fd = fopen(_filename.c_str(), "wb");
		if (!fd) {
			printError("FAIL");
			return false;
		}
		printSuccess("DONE");

		printInfo("Read flash ", false);

		if (_dump_format == "mcs") {
			/* Write Intel HEX format for flash dumps */
			size_t offset = 0;
			const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer.c_str());

			/* Emit Extended Linear Address records at every 64 KB boundary,
			 * matching Xilinx Impact behavior. Each data segment starts at
			 * address 0x0000 within the segment base set by the ELA record. */
			fprintf(fd, ":020000040000FA\n");

			while (offset < buffer.size()) {
				/* Emit Intel HEX records of 16 bytes */
				size_t remaining = buffer.size() - offset;
				uint8_t count = static_cast<uint8_t>(std::min(remaining, (size_t)16));
				uint16_t addr = static_cast<uint16_t>(offset);

				/* Compute checksum */
				uint8_t checksum = 0;
				checksum = (checksum + count) & 0xff;
				checksum = (checksum + (addr >> 8)) & 0xff;
				checksum = (checksum + (addr & 0xff)) & 0xff;
				/* Record type 00 = data */
				checksum = (checksum + 0) & 0xff;
				for (uint8_t i = 0; i < count; i++) {
					checksum = (checksum + data[offset + i]) & 0xff;
				}
				checksum = (~checksum) + 1;

				fprintf(fd, ":%02X%04X00", count, addr);
				for (uint8_t i = 0; i < count; i++) {
					fprintf(fd, "%02X", data[offset + i]);
				}
				fprintf(fd, "%02X\n", checksum);

				offset += count;

				/* If we just crossed a 64 KB boundary, emit a new ELA record */
				if (offset < buffer.size() && (offset & 0xFFFF) == 0) {
					uint16_t segment = static_cast<uint16_t>(offset >> 16);
					uint8_t cs = 0;
					cs = (cs + 2) & 0xff;           /* length */
					cs = (cs + 0) & 0xff;           /* addr hi */
					cs = (cs + 0) & 0xff;           /* addr lo */
					cs = (cs + 4) & 0xff;           /* type = Extended Linear Address */
					cs = (cs + (segment >> 8)) & 0xff; /* segment hi */
					cs = (cs + (segment & 0xff)) & 0xff; /* segment lo */
					cs = (~cs) + 1;
					fprintf(fd, ":02000004%04X%02X\n", segment, cs);
				}
			}
			/* End of file record */
			fprintf(fd, ":00000001FF\n");
		} else {
			/* Raw binary format */
			fwrite(buffer.c_str(), sizeof(uint8_t), buffer.size(), fd);
		}

		printSuccess("DONE");

		fclose(fd);

		return true;
	}

	if (_flash_chips & PRIMARY_FLASH) {
			select_flash_chip(PRIMARY_FLASH);
			FlashInterface::set_filename(_filename);
			if (!FlashInterface::dump(base_addr, len, _dump_format))
				return false;
		}
		if (_flash_chips & SECONDARY_FLASH) {
			select_flash_chip(SECONDARY_FLASH);
			FlashInterface::set_filename(_secondary_filename);
			if (!FlashInterface::dump(base_addr, len, _dump_format))
				return false;
		}

	return true;
}

bool Xilinx::detect_flash()
{
	if (_is_bpi_board) {
		if (!_bpi_flash) {
			if (!load_bpi_bridge())
				return false;
		}
		if (!_bpi_flash->detect()) {
			printError("BPI flash detection failed");
				return false;
		}
		return post_flash_access();
	}

	if (_flash_chips & PRIMARY_FLASH) {
		select_flash_chip(PRIMARY_FLASH);
		if (!FlashInterface::detect_flash())
			return false;
	}
	if (_flash_chips & SECONDARY_FLASH) {
		select_flash_chip(SECONDARY_FLASH);
		if (!FlashInterface::detect_flash())
			return false;
	}
	return true;
}

bool Xilinx::protect_flash(uint32_t len)
{
	if (_flash_chips & PRIMARY_FLASH) {
		select_flash_chip(PRIMARY_FLASH);
		if (!FlashInterface::protect_flash(len))
			return false;
	}
	if (_flash_chips & SECONDARY_FLASH) {
		select_flash_chip(SECONDARY_FLASH);
		if (!FlashInterface::protect_flash(len))
			return false;
	}
	return true;
}

bool Xilinx::unprotect_flash()
{
	if (_flash_chips & PRIMARY_FLASH) {
		select_flash_chip(PRIMARY_FLASH);
		if (!FlashInterface::unprotect_flash())
			return false;
	}
	if (_flash_chips & SECONDARY_FLASH) {
		select_flash_chip(SECONDARY_FLASH);
		if (!FlashInterface::unprotect_flash())
			return false;
	}
	return true;
}

bool Xilinx::set_quad_bit(bool set_quad)
{
	if (_flash_chips & PRIMARY_FLASH) {
		select_flash_chip(PRIMARY_FLASH);
		if (!FlashInterface::set_quad_bit(set_quad))
			return false;
	}
	if (_flash_chips & SECONDARY_FLASH) {
		select_flash_chip(SECONDARY_FLASH);
		if (!FlashInterface::set_quad_bit(set_quad))
			return false;
	}
	return true;
}

bool Xilinx::bulk_erase_flash()
{
	if (_flash_chips & PRIMARY_FLASH) {
		select_flash_chip(PRIMARY_FLASH);
		if (!FlashInterface::bulk_erase_flash())
			return false;
	}
	if (_flash_chips & SECONDARY_FLASH) {
		select_flash_chip(SECONDARY_FLASH);
		if (!FlashInterface::bulk_erase_flash())
			return false;
	}
	return true;
}


/*
 * jtag : jtag interface
 * cmd  : opcode for SPI flash
 * tx   : buffer to send
 * rx   : buffer to fill
 * len  : number of byte to send/receive (cmd not comprise)
 *        so to send only a cmd set len to 0 (or omit this param)
 */
int Xilinx::spi_put(uint8_t cmd,
			const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	/* SpiOverJtag v2 */
	if (_soj_is_v2)
		return spi_put_v2(cmd, tx, rx, len);

	int xfer_len = len + 1 + ((rx == NULL) ? 0 : 1);
	auto spi_put_v1_on_user = [&](const std::string &user_instruction,
			uint8_t *out) {
		uint8_t jtx[xfer_len];
		jtx[0] = McsParser::reverseByte(cmd);
		/* uint8_t jtx[xfer_len] = {McsParser::reverseByte(cmd)}; */
		uint8_t jrx[xfer_len];
		if (tx != NULL) {
			for (uint32_t i=0; i < len; i++)
				jtx[i+1] = McsParser::reverseByte(tx[i]);
		}
		/* addr BSCAN user1 */
		_jtag->shiftIR(get_ircode(_ircode_map, user_instruction), NULL,
			_irlen);
		/* send first already stored cmd,
		 * in the same time store each byte
		 * to next
		 */
		_jtag->shiftDR(jtx, (out == NULL)? NULL: jrx, 8*xfer_len);
		_jtag->flush();

		if (out != NULL) {
			for (uint32_t i=0; i < len; i++)
				out[i] = McsParser::reverseByte(jrx[i+1] >> 1) |
					(jrx[i+2] & 0x01);
		}
	};

	spi_put_v1_on_user(_user_instruction, rx);

	if (rx != NULL) {
		/* Try USER instruction fallback and SOJ v2 framing for all families
		 * when RDID response is not a valid JEDEC reply. Custom bridge
		 * bitstreams may use a different USER instruction than the default. */
		if (!_soj_is_v2 && cmd == 0x9F && len >= 3) {
			const bool v1_valid = looks_like_valid_jedec_reply(rx, len);
			if (!v1_valid) {
				const std::string saved_user_instruction =
					_user_instruction;
				const char *user_candidates[] = {
					"USER1", "USER2", "USER3", "USER4"
				};
				for (const char *candidate : user_candidates) {
					if (saved_user_instruction == candidate)
						continue;
					std::vector<uint8_t> rx_user(len, 0x00);
					_jtag->go_test_logic_reset();
					spi_put_v1_on_user(candidate, rx_user.data());
					if (_verbose > 0) {
						printf("SPI RDID probe %s:", candidate);
						for (uint32_t i = 0; i < len; i++)
							printf(" %02x", rx_user[i]);
						printf("\n");
					}
					if (looks_like_valid_jedec_reply(
							rx_user.data(), len)) {
						memcpy(rx, rx_user.data(), len);
						_user_instruction = candidate;
						if (_verbose > 0)
							printf("SPI RDID selected %s\n",
								candidate);
						return 0;
					}
				}
				_user_instruction = saved_user_instruction;
			}

			/* Some Spartan-6 bridges answer correctly only to the v2 packet
			 * format even when USER4 version probing did not decode to "2.0".
			 * Probe both framings for RDID and promote the session when v2 is
			 * the sane one. */
			std::vector<uint8_t> rx_v2(len, 0x00);
			_jtag->go_test_logic_reset();
			spi_put_v2(cmd, tx, rx_v2.data(), len);
			const bool v2_valid = looks_like_valid_jedec_reply(rx_v2.data(), len);

			if (_verbose > 0) {
				printf("SPI RDID probe v1:");
				for (uint32_t i = 0; i < len; i++)
					printf(" %02x", rx[i]);
				printf("\nSPI RDID probe v2:");
				for (uint32_t i = 0; i < len; i++)
					printf(" %02x", rx_v2[i]);
				printf("\n");
			}

			if (!v1_valid && v2_valid) {
				memcpy(rx, rx_v2.data(), len);
				_soj_is_v2 = true;
				if (_verbose > 0)
					printf("SPI RDID selected SOJ v2 framing\n");
			} else if (_fpga_family == SPARTAN6_FAMILY &&
					!looks_like_valid_jedec_reply(rx, len) &&
					!v2_valid) {
				/* XPCU control-transfer bitbang fallback is only relevant
				 * for Spartan-6 boards using that cable. */
				if (auto *ll = _jtag->get_ll_class();
						ll != nullptr && ll->setPreferControlBitbang(true)) {
					printWarn("Retrying Spartan-6 RDID probes through XPCU control-transfer JTAG");
					const std::string saved_user_instruction =
						_user_instruction;
					const char *user_candidates[] = {
						"USER1", "USER2", "USER3", "USER4"
					};

					_jtag->go_test_logic_reset();
					spi_put_v1_on_user(saved_user_instruction, rx);
					if (_verbose > 0) {
						printf("SPI RDID control probe %s:",
							saved_user_instruction.c_str());
						for (uint32_t i = 0; i < len; i++)
							printf(" %02x", rx[i]);
						printf("\n");
					}
					if (looks_like_valid_jedec_reply(rx, len))
						return 0;

					for (const char *candidate : user_candidates) {
						if (saved_user_instruction == candidate)
							continue;
						std::vector<uint8_t> rx_user(len, 0x00);
						_jtag->go_test_logic_reset();
						spi_put_v1_on_user(candidate, rx_user.data());
						if (_verbose > 0) {
							printf("SPI RDID control probe %s:",
								candidate);
							for (uint32_t i = 0; i < len; i++)
								printf(" %02x", rx_user[i]);
							printf("\n");
						}
						if (looks_like_valid_jedec_reply(
								rx_user.data(), len)) {
							memcpy(rx, rx_user.data(), len);
							_user_instruction = candidate;
							if (_verbose > 0)
								printf("SPI RDID selected %s in control-transfer mode\n",
									candidate);
							return 0;
						}
					}
					_user_instruction = saved_user_instruction;

					std::vector<uint8_t> rx_v2_control(len, 0x00);
					_jtag->go_test_logic_reset();
					spi_put_v2(cmd, tx, rx_v2_control.data(), len);
					if (_verbose > 0) {
						printf("SPI RDID control probe v2:");
						for (uint32_t i = 0; i < len; i++)
							printf(" %02x", rx_v2_control[i]);
						printf("\n");
					}
					if (looks_like_valid_jedec_reply(
							rx_v2_control.data(), len)) {
						memcpy(rx, rx_v2_control.data(), len);
						_soj_is_v2 = true;
						if (_verbose > 0)
							printf("SPI RDID selected SOJ v2 framing in control-transfer mode\n");
						return 0;
					}

					ll->setPreferControlBitbang(false);
				}
			}
		}
	}
	return 0;
}

int Xilinx::spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len)
{
	int xfer_len = len + ((rx == NULL) ? 0 : 1);
	uint8_t jtx[xfer_len];
	uint8_t jrx[xfer_len];
	if (tx != NULL) {
		for (uint32_t i=0; i < len; i++)
			jtx[i] = McsParser::reverseByte(tx[i]);
	}
	/* addr BSCAN user1 */
	_jtag->shiftIR(get_ircode(_ircode_map, _user_instruction), NULL, _irlen);
	/* send first already stored cmd,
	 * in the same time store each byte
	 * to next
	 */
	_jtag->shiftDR(jtx, (rx == NULL)? NULL: jrx, 8*xfer_len);
	_jtag->flush();

	if (rx != NULL) {
		for (uint32_t i=0; i < len; i++)
			rx[i] = McsParser::reverseByte(jrx[i] >> 1) | (jrx[i+1] & 0x01);
	}
	return 0;
}

int Xilinx::spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
			uint32_t timeout, bool verbose)
{
	uint8_t rx[2];
	uint8_t tx[2];
	uint8_t tmp;
	uint32_t count = 0;
	const uint8_t shift = _jtag_chain_len;
	uint8_t idx = 0;

	if (_soj_is_v2)
		tx[idx++] = (0x2 << 1) | 1;
	tx[idx++] = McsParser::reverseByte(cmd);

	_jtag->shiftIR(get_ircode(_ircode_map, _user_instruction), NULL, _irlen, Jtag::UPDATE_IR);
	_jtag->shiftDR(tx, NULL, 8 * idx, Jtag::SHIFT_DR);

	do {
		_jtag->shiftDR(tx, rx, 8*2, Jtag::SHIFT_DR);
		tmp = McsParser::reverseByte(rx[0 ]>> shift);
		if (shift == 1)
			tmp |= (0x01 & rx[1]);
		else
			tmp |= McsParser::reverseByte(rx[1]) >> (8 - shift);

		count++;
		if (count == timeout){
			printf("timeout: %x %x %x\n", tmp, rx[0], rx[1]);
			break;
		}
		if (verbose) {
			printf("%x %x %x %u %02x %02x\n", tmp, mask, cond, count, rx[0], rx[1]);
		}
	} while ((tmp & mask) != cond);
	_jtag->shiftDR(tx, rx, 8 * 2, Jtag::EXIT1_DR);
	_jtag->go_test_logic_reset();

	if (count == timeout) {
		printf("%x\n", tmp);
		std::cout << "wait: Error" << std::endl;
		return -ETIME;
	} else {
		return 0;
	}
}

int Xilinx::spi_put_v2(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
		uint32_t len)
{
	const uint32_t real_len = len + 1;  // rx/tx length + cmd
	uint32_t kPktLen = real_len + 2;  // One header and +1 due to the needs of an additional bit/byte
	uint8_t mode = 0x01;
	if (real_len > 32) {
		kPktLen++;  // Additional header
		mode = 0x00;
	}

	const uint32_t xfer_bit_len = (kPktLen - 1) * 8 + (rx ? 8 : 1);

	uint8_t jrx[kPktLen];
	uint8_t pkt[kPktLen];
	uint32_t idx = 0;

	pkt[idx++] = ((0x1f & real_len) << 3) | ((0x03 & mode) << 1) | 1;
	if (mode == 0x00)
		pkt[idx++] = 0xff & (real_len >> 5);

	pkt[idx++] = McsParser::reverseByte(cmd);
	if (tx) {
		for (uint32_t i=0; i < len; i++)
			pkt[idx++] = McsParser::reverseByte(tx[i]);
	} else {
		memset(&pkt[idx], 0, len);
		idx += len;
	}

	/* addr BSCAN user1 */
	_jtag->shiftIR(get_ircode(_ircode_map, _user_instruction), NULL, _irlen);
	_jtag->shiftDR(pkt, (rx == NULL) ? NULL : jrx, xfer_bit_len);
	_jtag->go_test_logic_reset();
	_jtag->flush();

	if (_verbose) {
		for (uint32_t i = 0; i < kPktLen; i++)
			printf("%02x ", pkt[i]);
		printf("\n");
	}

	if (rx != NULL) {
		if (_verbose) {
			for (uint32_t i = 0; i < kPktLen; i++)
				printf("%02x ", jrx[i]);
			printf("\n");
			for (uint32_t i = 0; i < kPktLen; i++)
				printf("%02x ", McsParser::reverseByte(jrx[i]));
			printf("\n");
		}
		idx = (mode == 0 ? 3 : 2);
		for (uint32_t i = 0; i < len; i++) {
			rx[i] = McsParser::reverseByte(jrx[i + idx]);
		}

		if (_verbose) {
			for (uint32_t i = 0; i < len; i++)
				printf("%02x ", rx[i]);
			printf("\n");
		}

		if (_fpga_family == SPARTAN6_FAMILY &&
				looks_like_invalid_bridge_reply(rx, len)) {
			if (auto *ll = _jtag->get_ll_class();
					ll != nullptr && ll->selectAlternateTdoMask()) {
				printWarn("Retrying Spartan-6 spiOverJtag transfer with alternate XPCU TDO mask");
				const int ret = spi_put_v2(cmd, tx, rx, len);
				if (looks_like_invalid_bridge_reply(rx, len))
					ll->restorePrimaryTdoMask();
				return ret;
			}
		}
	}

	return 0;
}

void Xilinx::select_flash_chip(xilinx_flash_chip_t flash_chip) {
	switch (flash_chip) {
	case SECONDARY_FLASH:
		_user_instruction = "USER2";
		break;
	case PRIMARY_FLASH:
	default:
		_user_instruction = "USER1";
		break;
	}
}
