// SPI Flash erase-and-program operations
#include "protocols/spiFlash.hpp"
#include <cstring>
#include "utils/display.hpp"
#include "utils/progressBar.hpp"

bool SPIFlash::erase_and_prog(const std::vector<FlashDataSection> &sections, bool full_erase)
{
	uint32_t len = 0, flash_len;
	uint32_t base_addr = 0;
	/* For full erase and to check BP: consider the full flash size */
	if (full_erase) {
		flash_len = _flash_model->nr_sector * 0x10000;
	} else { /* not a full erase: consider bottom base_addr and upper address */
		base_addr = sections.front().getStartAddr();
		flash_len = sections.back().getCurrentAddr() - base_addr;
	}

	/* Compute real length to be written */
	for (const FlashDataSection &sec: sections)
		len += sec.getLength();

	/* Sanity check / disables protection */
	if (!prepare_flash(base_addr, flash_len))
		return false;

	/* instead of sector erase => perform a full flash erase */
	if (full_erase) {
		if (bulk_erase(true, true) == -1)
			return false;
	} else {
		printInfo("Erase Flash: ", false);
		if (sectors_erase(base_addr, len) == -1) {
			printError("FAIL");
			return -1;
		} else {
			printSuccess("DONE");
		}
	}

	ProgressBar progress("Writing", len, 50, _verbose < 0);
	uint32_t len_done = 0;
	for (const FlashDataSection &sec: sections) {
		int size = 0;
		/* prepare section write */
		const uint32_t base_addr = sec.getStartAddr(); // start address
		const uint32_t sec_len = sec.getLength(); // section length
		const uint8_t *ptr = sec.getRecord().data(); // data
		for (uint32_t addr = 0; addr < sec_len; addr += size, ptr+=size, len_done+=size) {
			size = (addr + 256 > sec_len) ? (sec_len - addr) : 256;
			if ((_jedec_id >> 8) == 0xbf258d) {
				size = 1;
			}
			if (write_page(base_addr + addr, ptr, size) == -1)
				return -1;
			progress.display(len_done);
		}
	}
	progress.done();

	/* and if required: relock blocks */
	if (_must_relock) {
		enable_protection(_status);
		if (_verbose > 0)
			display_status_reg(read_status_reg());
	}
	return true;
}

int SPIFlash::erase_and_prog(int base_addr, const uint8_t *data, int len)
{
	if (!prepare_flash(base_addr, len))
		return -1;

	/* Now we can erase sector and write new data */
	ProgressBar progress("Writing", len, 50, _verbose < 0);
	if (sectors_erase(base_addr, len) == -1)
		return -1;

	const uint8_t *ptr = data;
	int size = 0;
	for (int addr = 0; addr < len; addr += size, ptr+=size) {
		size = (addr + 256 > len)?(len-addr) : 256;
		if ((_jedec_id >> 8) == 0xbf258d) {
			size = 1;
		}
		if (write_page(base_addr + addr, ptr, size) == -1)
			return -1;
		progress.display(addr);
	}
	progress.done();

	/* and if required: relock blocks */
	if (_must_relock) {
		enable_protection(_status);
		if (_verbose > 0)
			display_status_reg(read_status_reg());
	}
	return 0;
}

bool SPIFlash::verify(const int &base_addr, const uint8_t *data,
		const int &len, int rd_burst)
{
	if (rd_burst == 0) {
		rd_burst = len;
		if (rd_burst > 65536)
			rd_burst = 65536;
	}

	printInfo("Verifying write (May take time)");

	std::string verify_data;
	verify_data.resize(rd_burst);

	ProgressBar progress("Reading", len, 50, false);
	for (int i = 0; i < len; i += rd_burst) {
		if (rd_burst + i > len)
			rd_burst = len - i;
		if (0 != read(base_addr + i, (uint8_t*)&verify_data[0], rd_burst)) {
			progress.fail();
			printError("Failed to read flash");
			return false;
		}


		for (int ii = 0; ii < rd_burst; ii++) {
			if ((uint8_t)verify_data[ii] != data[i+ii]) {
				progress.fail();
				printError("Verification failed at " +
						std::to_string(base_addr + i + ii));
				return false;
			}
		}
		progress.display(i);
	}

	progress.done();

	return true;
}

void SPIFlash::reset()
{
	uint8_t data[8];
	memset(data, 0xff, 8);
	_spi->spi_put(0xff, data, NULL, 8);
	_spi->spi_put(FLASH_RSTEN, NULL, NULL, 0);
	_spi->spi_put(FLASH_RST, NULL, NULL, 0);
}
