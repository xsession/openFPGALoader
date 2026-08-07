// Xilinx XCF flow programming
#include "vendors/xilinx.hpp"
#include "utils/progressBar.hpp"
#include <unistd.h>
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


#define BYPASS      0xff

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

void Xilinx::xcf_flow_enable(uint8_t mode)
{
	if (get_xcf_prom_info(_jtag->get_target_device_id()).xcfp) {
		xcfp_flow_enable();
		return;
	}
	xcf_shift_ir(_jtag, XCF_ISC_ENABLE, _irlen);
	_jtag->shiftDR(&mode, NULL, 6);
	_jtag->toggleClk(1);
}

void Xilinx::xcf_flow_disable()
{
	if (get_xcf_prom_info(_jtag->get_target_device_id()).xcfp) {
		xcfp_flow_disable();
		return;
	}
	xcf_shift_ir(_jtag, XCF_ISC_DISABLE, _irlen);
	_jtag->flush();
	usleep(110000);
	xcf_shift_ir(_jtag, BYPASS, _irlen);
	_jtag->toggleClk(1);
}

bool Xilinx::xcf_flow_erase()
{
	const xcf_prom_info prom_info = get_xcf_prom_info(_jtag->get_target_device_id());
	if (prom_info.xcfp) {
		const uint8_t mask = static_cast<uint8_t>((1u << prom_info.array_count) - 1u);
		return xcfp_flow_erase(mask);
	}
	uint8_t xfer_buf[2] = {0x01, 0x00};

	printInfo("Erase flash ", false);
	xcf_flow_enable();

	xcf_shift_ir(_jtag, XCF_ISC_ADDR_SHIFT, _irlen);
	_jtag->shiftDR(xfer_buf, NULL, 16);
	_jtag->toggleClk(1);

	xcf_shift_ir(_jtag, XCF_ISC_ERASE, _irlen);
	_jtag->flush();
	usleep(500000);

	int i;
	for (i = 0; i < 32; i++) {
		xcf_shift_ir(_jtag, XCF_ISCTESTSTATUS, _irlen);
		_jtag->flush();
		usleep(500000);
		_jtag->shiftDR(NULL, xfer_buf, 8);
		if ((xfer_buf[0] & 0x04))
			break;
	}

	if (i == 32) {
		printError("FAIL");
		return false;
	}

	printSuccess("DONE");

	xcf_flow_disable();

	return true;
}

bool Xilinx::xcf_program(ConfigBitstreamParser *bitfile)
{
	if (!bitfile)
		throw std::runtime_error("called with null bitstream");
	const xcf_prom_info prom_info = get_xcf_prom_info(_jtag->get_target_device_id());
	if (prom_info.xcfp)
		return xcfp_program(bitfile);

	uint8_t tx_buf[4096 / 8];
	const uint16_t pkt_len = prom_info.pkt_len;
	const uint8_t *data = bitfile->getData();
	uint32_t data_len = bitfile->getLength() / 8;
	uint32_t xfer_len, offset = 0;
	uint32_t addr = 0;
	Jtag::tapState_t xfer_end;

	if (data_len > prom_info.capacity) {
		throw std::runtime_error("bitstream is larger than selected XCF PROM capacity");
	}

	/* limit JTAG clock frequency to 15MHz */
	if (_jtag->getClkFreq() > 15e6)
		_jtag->setClkFreq(15e6);

	if (!xcf_flow_erase()) {
		printError("flow erase failed");
		return false;
	}

	xcf_flow_enable();

	int blk_id = 0;

	ProgressBar progress("Write PROM", (data_len / pkt_len), 50, _quiet);

	while (data_len > 0) {
		if (data_len < pkt_len) {
			xfer_len = data_len;
			xfer_end = Jtag::SHIFT_DR;
		} else {
			xfer_len = pkt_len;
			xfer_end = Jtag::RUN_TEST_IDLE;
		}

		/* send data to PROM */
		xcf_shift_ir(_jtag, XCF_ISC_DATA_SHIFT, _irlen);
		_jtag->shiftDR(data+offset, NULL, xfer_len * 8, xfer_end);
		if (xfer_len != pkt_len) {
			uint32_t res = pkt_len - xfer_len;
			memset(tx_buf, 0xff, res);
			_jtag->shiftDR(tx_buf, NULL, res * 8);
		}

		_jtag->toggleClk(1);

		/* send address */
		tx_buf[0] = (addr >> 0) & 0x00ff;
		tx_buf[1] = (addr >> 8) & 0x00ff;
		xcf_shift_ir(_jtag, XCF_ISC_ADDR_SHIFT, _irlen);
		_jtag->shiftDR(tx_buf, NULL, 16);
		_jtag->toggleClk(1);

		/* send program instruction */
		xcf_shift_ir(_jtag, XCF_ISC_PROGRAM, _irlen);
		_jtag->flush();
		usleep((addr == 0) ? 14000: 500);

		/* wait until bit 3 != 1 */
		int i;
		for (i = 0; i < 29; i++) {
			xcf_shift_ir(_jtag, XCF_ISCTESTSTATUS, _irlen);
			_jtag->flush();
			usleep(500);
			_jtag->shiftDR(NULL, tx_buf, 8);
			if ((tx_buf[0] & 0x04))
				break;
		}

		if (i == 29) {
			progress.fail();
			return false;
		}

		blk_id++;
		offset += xfer_len;
		addr += 32;
		data_len -= xfer_len;
		progress.display(blk_id);
	}
	progress.done();

	/* program done */
	xcf_shift_ir(_jtag, BYPASS, _irlen);
	_jtag->toggleClk(1);

	if (_verify) {
		std::string flash = xcf_read();
		uint32_t file_size = bitfile->getLength() / 8;
		uint32_t prom_size = (uint32_t)flash.size();

		uint32_t nb_bytes = (file_size > prom_size) ? prom_size : file_size;
		ProgressBar progress2("Verify Flash", nb_bytes, 50, _quiet);

		for (uint32_t pos = 0; pos < nb_bytes; pos++) {
			if (data[pos] != (uint8_t)flash[pos]) {
				progress2.fail();
				char error[64];
				snprintf(error, sizeof(error),
						"Error: wrong value: read %02x instead of %02x",
						(uint8_t)flash[pos], (uint8_t)data[pos]);
				printError(error);
				xcf_flow_disable();
				return false;
			}
			progress.display(pos);
		}
		progress2.done();
	}

	_jtag->go_test_logic_reset();

	xcf_flow_disable();

	/* reconfigure FPGA */
	xcf_shift_ir(_jtag, XCF_CONFIG, _irlen);
	_jtag->toggleClk(1);
	xcf_shift_ir(_jtag, BYPASS, _irlen);
	_jtag->toggleClk(1);

	return true;
}

std::string Xilinx::xcf_read()
{
	uint32_t addr = 0;
	uint8_t rx_buf[4096 / 8];
	const xcf_prom_info prom_info = get_xcf_prom_info(_jtag->get_target_device_id());
	if (prom_info.xcfp)
		return xcfp_read();

	const uint16_t pkt_len = prom_info.pkt_len;
	const uint32_t nb_section = prom_info.nb_section;
	std::string buffer;

	/* limit JTAG clock frequency to 15MHz */
	if (_jtag->getClkFreq() > 15e6)
		_jtag->setClkFreq(15e6);

	ProgressBar progress("Read PROM", nb_section, 50, _quiet);

	for (size_t section = 0; section < nb_section; section++) {
		/* send address */
		rx_buf[0] = (addr >> 0) & 0x00ff;
		rx_buf[1] = (addr >> 8) & 0x00ff;
		xcf_shift_ir(_jtag, XCF_ISC_ADDR_SHIFT, _irlen);
		_jtag->shiftDR(rx_buf, NULL, 16);
		_jtag->toggleClk(1);

		/* send data to PROM */
		xcf_shift_ir(_jtag, XCF_ISC_READ, _irlen);
		_jtag->flush();
		usleep(50);
		_jtag->shiftDR(NULL, rx_buf, pkt_len * 8);

		for (int i = 0; i < pkt_len; i++)
			buffer += rx_buf[i];

		progress.display(section);
		addr += 32;
	}
	progress.done();

	return buffer;
}

/*--------------------------------------------------------*/
/*--------------------------------------------------------*/