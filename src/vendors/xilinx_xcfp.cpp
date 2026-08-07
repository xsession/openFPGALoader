// Xilinx XCFP flow programming
#include "vendors/xilinx.hpp"
#include "utils/progressBar.hpp"
#include <unistd.h>
#include <iomanip>
#define XCFP_ISC_READ      0xF8
#define XCFP_IDCODE        0x00FE
#define XCFP_BYPASS        0xFFFF
#define XCFP_XSC_UNLOCK    0xAA55
#define XCFP_XSC_DATA_BTC  0x00F2
#define XCFP_XSC_DATA_CCB  0x000C
#define XCFP_XSC_DATA_SUCR 0x000E
#define XCFP_XSC_DATA_DONE 0x0009


#define XCFP_ISC_READ      0xF8
#define XCFP_IDCODE        0x00FE
#define XCFP_BYPASS        0xFFFF
#define XCFP_XSC_UNLOCK    0xAA55
#define XCFP_XSC_DATA_BTC  0x00F2
#define XCFP_XSC_DATA_CCB  0x000C
#define XCFP_XSC_DATA_SUCR 0x000E
#define XCFP_XSC_DATA_DONE 0x0009

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

static constexpr uint32_t XCFP_ARRAY_SIZE = 0x100000;
static constexpr uint32_t XCFP_FRAME_SIZE = 32;

struct xcf_prom_info {
	uint16_t pkt_len;
	uint32_t nb_section;
	uint32_t capacity;
	uint8_t array_count;
	bool xcfp;
};

static xcf_prom_info get_xcf_prom_info(uint32_t idcode)
{
	/* Ignore the four-bit silicon revision field. */
	switch (idcode & 0x0fffffff) {
	case 0x05044093: /* XCF01S: 1 Mbit */
		return {2048 / 8, 512, 1 * 1024 * 1024 / 8, 0, false};
	case 0x05045093: /* XCF02S: 2 Mbit */
		return {4096 / 8, 512, 2 * 1024 * 1024 / 8, 0, false};
	case 0x05046093: /* XCF04S: 4 Mbit */
		return {4096 / 8, 1024, 4 * 1024 * 1024 / 8, 0, false};
	case 0x05057093: /* XCF08P: 8 Mbit */
		return {XCFP_FRAME_SIZE, 32768, 1 * XCFP_ARRAY_SIZE, 1, true};
	case 0x05058093: /* XCF16P: 16 Mbit */
		return {XCFP_FRAME_SIZE, 65536, 2 * XCFP_ARRAY_SIZE, 2, true};
	case 0x05059093: /* XCF32P: 32 Mbit */
		return {XCFP_FRAME_SIZE, 131072, 4 * XCFP_ARRAY_SIZE, 4, true};
	default:
		throw std::runtime_error("unsupported XCF PROM IDCODE");
	}
}

static void xcf_shift_ir(Jtag *jtag, uint16_t instruction, int irlen,
		Jtag::tapState_t end_state = Jtag::RUN_TEST_IDLE)
{
	uint8_t ir[2] = {
		static_cast<uint8_t>(instruction & 0xff),
		static_cast<uint8_t>((instruction >> 8) & 0xff)
	};
	jtag->shiftIR(ir, NULL, irlen, end_state);
}

void Xilinx::xcfp_flow_enable()
{
	uint8_t mode = 0x00;
	xcf_shift_ir(_jtag, XCF_ISC_ENABLE, _irlen);
	_jtag->shiftDR(&mode, NULL, 8);
	_jtag->toggleClk(1);
}

void Xilinx::xcfp_flow_disable()
{
	xcf_shift_ir(_jtag, XCF_ISC_DISABLE, _irlen);
	_jtag->flush();
	usleep(1000);
	xcf_shift_ir(_jtag, XCFP_BYPASS, _irlen);
	_jtag->toggleClk(1);
	_jtag->go_test_logic_reset();
}

bool Xilinx::xcfp_verify_idcode()
{
	uint8_t value[4] = {0, 0, 0, 0};
	xcf_shift_ir(_jtag, XCFP_IDCODE, _irlen);
	_jtag->toggleClk(1);
	_jtag->shiftDR(NULL, value, 32);
	const uint32_t read_id = static_cast<uint32_t>(value[0]) |
		(static_cast<uint32_t>(value[1]) << 8) |
		(static_cast<uint32_t>(value[2]) << 16) |
		(static_cast<uint32_t>(value[3]) << 24);
	const uint32_t expected_id = _jtag->get_target_device_id();
	if ((read_id & 0x0fffffff) == (expected_id & 0x0fffffff))
		return true;

	std::ostringstream error;
	error << "XCFP IDCODE mismatch: read 0x" << std::hex << std::setw(8)
		<< std::setfill('0') << read_id << ", expected 0x" << std::setw(8)
		<< expected_id;
	printError(error.str());
	return false;
}

bool Xilinx::xcfp_wait_ready(uint32_t polls, uint32_t poll_delay_us,
		const char *operation)
{
	uint8_t status = 0;
	for (uint32_t poll = 0; poll < polls; poll++) {
		xcf_shift_ir(_jtag, XCF_ISCTESTSTATUS, _irlen);
		_jtag->shiftDR(NULL, &status, 8);
		if (status & 0x04) {
			if (status == 0x36)
				return true;
			std::ostringstream error;
			error << "XCFP " << operation << " failed with status 0x"
				<< std::hex << std::setw(2) << std::setfill('0')
				<< static_cast<unsigned>(status);
			printError(error.str());
			return false;
		}
		if (poll_delay_us != 0) {
			_jtag->flush();
			usleep(poll_delay_us);
		}
	}

	std::ostringstream error;
	error << "timeout while waiting for XCFP " << operation;
	printError(error.str());
	return false;
}

bool Xilinx::xcfp_flow_erase(uint8_t array_mask)
{
	const xcf_prom_info prom_info = get_xcf_prom_info(_jtag->get_target_device_id());
	if (!prom_info.xcfp)
		throw std::runtime_error("XCFP erase called for an XCF-S device");
	const uint8_t valid_mask = static_cast<uint8_t>((1u << prom_info.array_count) - 1u);
	array_mask &= valid_mask;
	if (array_mask == 0) {
		printError("XCFP erase requested with an empty array mask");
		return false;
	}

	printInfo("Erase XCFP arrays ", false);
	_jtag->go_test_logic_reset();
	_jtag->flush();
	usleep(1000);
	if (!xcfp_verify_idcode()) {
		printError("FAIL");
		return false;
	}
	xcfp_flow_enable();

	uint8_t address[3] = {
		static_cast<uint8_t>(0x30 | array_mask), 0x00, 0x00
	};
	xcf_shift_ir(_jtag, XCFP_XSC_UNLOCK, _irlen);
	_jtag->shiftDR(address, NULL, 24);
	_jtag->toggleClk(1);

	/*
	 * XCFxxP erase must transition from the erase instruction directly to
	 * its data register.  Ending IR at EXIT1_IR avoids RTI/Pause states.
	 */
	xcf_shift_ir(_jtag, XCF_ISC_ERASE, _irlen, Jtag::EXIT1_IR);
	_jtag->shiftDR(address, NULL, 24);
	_jtag->flush();
	usleep(500000);
	const bool ok = xcfp_wait_ready(280, 500000, "erase");
	xcfp_flow_disable();
	if (!ok) {
		printError("FAIL");
		return false;
	}

	printSuccess("DONE");
	return true;
}

bool Xilinx::xcfp_program_register(uint16_t instruction, const uint8_t *value,
		uint32_t bit_length, const char *name)
{
	if (!value || bit_length == 0 || bit_length > 32 || (bit_length & 7))
		throw std::runtime_error("invalid XCFP register programming request");
	uint8_t readback[4] = {0, 0, 0, 0};
	const uint32_t byte_length = bit_length / 8;

	xcf_shift_ir(_jtag, instruction, _irlen);
	_jtag->shiftDR(value, NULL, bit_length);
	_jtag->toggleClk(1);
	xcf_shift_ir(_jtag, XCF_ISC_PROGRAM, _irlen);
	_jtag->flush();
	usleep(1000);

	xcf_shift_ir(_jtag, instruction, _irlen);
	_jtag->toggleClk(1);
	_jtag->shiftDR(NULL, readback, bit_length);
	if (memcmp(value, readback, byte_length) == 0)
		return true;

	std::ostringstream error;
	error << "XCFP " << name << " register verification failed";
	printError(error.str());
	return false;
}

bool Xilinx::xcfp_verify(const uint8_t *data, uint32_t data_len,
		uint8_t used_arrays)
{
	const uint32_t total_frames = (data_len + XCFP_FRAME_SIZE - 1) /
		XCFP_FRAME_SIZE;
	const uint32_t frames_per_array = XCFP_ARRAY_SIZE / XCFP_FRAME_SIZE;
	uint8_t address[3] = {0, 0, 0};
	uint8_t readback[XCFP_FRAME_SIZE];
	uint32_t offset = 0;
	uint32_t frame_index = 0;
	ProgressBar progress("Verify XCFP", total_frames, 50, _quiet);

	for (uint8_t array = 0; array < used_arrays; array++) {
		const uint32_t array_address = static_cast<uint32_t>(array) * XCFP_ARRAY_SIZE;
		address[0] = static_cast<uint8_t>(array_address & 0xff);
		address[1] = static_cast<uint8_t>((array_address >> 8) & 0xff);
		address[2] = static_cast<uint8_t>((array_address >> 16) & 0xff);
		xcf_shift_ir(_jtag, XCF_ISC_ADDR_SHIFT, _irlen);
		_jtag->shiftDR(address, NULL, 24);
		_jtag->toggleClk(1);

		for (uint32_t frame = 0;
				frame < frames_per_array && offset < data_len; frame++) {
			xcf_shift_ir(_jtag, XCFP_ISC_READ, _irlen);
			_jtag->flush();
			usleep(25);
			xcf_shift_ir(_jtag, XCF_ISC_DATA_SHIFT, _irlen);
			_jtag->toggleClk(1);
			_jtag->shiftDR(NULL, readback, XCFP_FRAME_SIZE * 8);

			const uint32_t compare_len = std::min<uint32_t>(
				XCFP_FRAME_SIZE, data_len - offset);
			for (uint32_t pos = 0; pos < compare_len; pos++) {
				if (readback[pos] != data[offset + pos]) {
					progress.fail();
					std::ostringstream error;
					error << "XCFP verify mismatch at byte 0x" << std::hex
						<< (offset + pos) << ": read 0x" << std::setw(2)
						<< std::setfill('0') << static_cast<unsigned>(readback[pos])
						<< ", expected 0x" << std::setw(2)
						<< static_cast<unsigned>(data[offset + pos]);
					printError(error.str());
					return false;
				}
			}
			offset += compare_len;
			progress.display(frame_index++);
		}
	}
	progress.done();
	return offset == data_len;
}

bool Xilinx::xcfp_program(ConfigBitstreamParser *bitfile)
{
	if (!bitfile)
		throw std::runtime_error("called with null bitstream");
	const xcf_prom_info prom_info = get_xcf_prom_info(_jtag->get_target_device_id());
	if (!prom_info.xcfp)
		throw std::runtime_error("XCFP program called for an XCF-S device");
	const uint8_t *data = bitfile->getData();
	const uint32_t data_len = bitfile->getLength() / 8;
	if (!data || data_len == 0)
		throw std::runtime_error("empty XCFP bitstream");
	if (data_len > prom_info.capacity)
		throw std::runtime_error("bitstream is larger than selected XCFP capacity");

	if (_jtag->getClkFreq() > 15e6)
		_jtag->setClkFreq(15e6);
	const uint8_t used_arrays = static_cast<uint8_t>(
		(data_len + XCFP_ARRAY_SIZE - 1) / XCFP_ARRAY_SIZE);
	const uint8_t erase_mask = static_cast<uint8_t>(
		(1u << prom_info.array_count) - 1u);
	if (!xcfp_flow_erase(erase_mask))
		return false;

	xcfp_flow_enable();
	const uint32_t total_frames = (data_len + XCFP_FRAME_SIZE - 1) /
		XCFP_FRAME_SIZE;
	ProgressBar progress("Write XCFP", total_frames, 50, _quiet);
	uint8_t frame[XCFP_FRAME_SIZE];
	uint8_t address[3] = {0, 0, 0};
	uint32_t offset = 0;
	uint32_t frame_index = 0;
	while (offset < data_len) {
		memset(frame, 0xff, sizeof(frame));
		const uint32_t write_len = std::min<uint32_t>(
			XCFP_FRAME_SIZE, data_len - offset);
		memcpy(frame, data + offset, write_len);

		xcf_shift_ir(_jtag, XCF_ISC_DATA_SHIFT, _irlen);
		_jtag->shiftDR(frame, NULL, XCFP_FRAME_SIZE * 8);
		_jtag->toggleClk(1);

		const bool first_frame_in_array = (offset % XCFP_ARRAY_SIZE) == 0;
		if (first_frame_in_array) {
			address[0] = static_cast<uint8_t>(offset & 0xff);
			address[1] = static_cast<uint8_t>((offset >> 8) & 0xff);
			address[2] = static_cast<uint8_t>((offset >> 16) & 0xff);
			xcf_shift_ir(_jtag, XCF_ISC_ADDR_SHIFT, _irlen);
			_jtag->shiftDR(address, NULL, 24);
			_jtag->toggleClk(1);
		}

		xcf_shift_ir(_jtag, XCF_ISC_PROGRAM, _irlen);
		_jtag->flush();
		usleep(first_frame_in_array ? 1000 : 25);
		if (!xcfp_wait_ready(100, 25, "program")) {
			progress.fail();
			xcfp_flow_disable();
			return false;
		}

		offset += write_len;
		progress.display(frame_index++);
	}
	progress.done();

	const uint32_t btc = 0xffffffe0u |
		(static_cast<uint32_t>(used_arrays - 1u) << 2);
	const uint8_t btc_value[4] = {
		static_cast<uint8_t>(btc & 0xff),
		static_cast<uint8_t>((btc >> 8) & 0xff),
		static_cast<uint8_t>((btc >> 16) & 0xff),
		static_cast<uint8_t>((btc >> 24) & 0xff)
	};
	/* Slave serial PROM mode, fast external clock; FPGA is master serial. */
	const uint8_t ccb_value[2] = {0xff, 0xff};
	const uint8_t sucr_value[2] = {0xfc, 0xff};
	const uint8_t done_value[1] = {
		static_cast<uint8_t>(0xc0 | (0x0f & (0x0f << prom_info.array_count)))
	};
	if (!xcfp_program_register(XCFP_XSC_DATA_BTC, btc_value, 32, "BTC") ||
			!xcfp_program_register(XCFP_XSC_DATA_CCB, ccb_value, 16, "CCB") ||
			!xcfp_program_register(XCFP_XSC_DATA_SUCR, sucr_value, 16, "SUCR") ||
			!xcfp_program_register(XCFP_XSC_DATA_DONE, done_value, 8, "DONE")) {
		xcfp_flow_disable();
		return false;
	}

	if (_verify && !xcfp_verify(data, data_len, used_arrays)) {
		xcfp_flow_disable();
		return false;
	}
	xcfp_flow_disable();

	/* Start a configuration cycle if a downstream FPGA is present. */
	xcf_shift_ir(_jtag, XCF_CONFIG, _irlen);
	_jtag->toggleClk(1);
	xcf_shift_ir(_jtag, XCFP_BYPASS, _irlen);
	_jtag->toggleClk(1);
	_jtag->go_test_logic_reset();
	return true;
}

std::string Xilinx::xcfp_read()
{
	const xcf_prom_info prom_info = get_xcf_prom_info(_jtag->get_target_device_id());
	if (!prom_info.xcfp)
		throw std::runtime_error("XCFP read called for an XCF-S device");
	if (_jtag->getClkFreq() > 15e6)
		_jtag->setClkFreq(15e6);

	std::string buffer;
	buffer.reserve(prom_info.capacity);
	const uint32_t frames_per_array = XCFP_ARRAY_SIZE / XCFP_FRAME_SIZE;
	const uint32_t total_frames = prom_info.capacity / XCFP_FRAME_SIZE;
	ProgressBar progress("Read XCFP", total_frames, 50, _quiet);
	uint8_t address[3] = {0, 0, 0};
	uint8_t readback[XCFP_FRAME_SIZE];
	uint32_t frame_index = 0;
	for (uint8_t array = 0; array < prom_info.array_count; array++) {
		const uint32_t array_address = static_cast<uint32_t>(array) * XCFP_ARRAY_SIZE;
		address[0] = static_cast<uint8_t>(array_address & 0xff);
		address[1] = static_cast<uint8_t>((array_address >> 8) & 0xff);
		address[2] = static_cast<uint8_t>((array_address >> 16) & 0xff);
		xcf_shift_ir(_jtag, XCF_ISC_ADDR_SHIFT, _irlen);
		_jtag->shiftDR(address, NULL, 24);
		_jtag->toggleClk(1);

		for (uint32_t frame = 0; frame < frames_per_array; frame++) {
			xcf_shift_ir(_jtag, XCFP_ISC_READ, _irlen);
			_jtag->flush();
			usleep(25);
			xcf_shift_ir(_jtag, XCF_ISC_DATA_SHIFT, _irlen);
			_jtag->toggleClk(1);
			_jtag->shiftDR(NULL, readback, XCFP_FRAME_SIZE * 8);
			buffer.append(reinterpret_cast<const char *>(readback),
				XCFP_FRAME_SIZE);
			progress.display(frame_index++);
		}
	}
	progress.done();
	return buffer;
}