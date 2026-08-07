// SPI Flash erase operations
#include "protocols/spiFlash.hpp"
#include <cstring>
#include "utils/display.hpp"
#include "utils/progressBar.hpp"

int SPIFlash::bulk_erase(bool verbose, bool skip_bp_check)
{
	int ret = 0, ret2 = 0;
	uint8_t bp = 0;
	uint32_t timeout=1000000;
	if (!skip_bp_check) {
		if (verbose)
			printInfo("Check Flash Protection: ", false);
		bp = get_bp();
		if (bp != 0) {
			if (!_unprotect) {
				printError("FAIL");
				printError("Error: Can't erase flash: block protection is set");
				printError("       can't unlock without --unprotect-flash");
				return -1;
			}

			ret = disable_protection();
		}

		if (verbose) {
			if (ret == 0)
				printSuccess("DONE");
			else
				printError("FAIL");
		}

		if (ret != 0)
			return ret;
	}

	if (verbose)
		printInfo("Bulk erase: ", false);
	if ((ret = write_enable()) != 0) {
		printError("FAIL");
		return ret;
	}
	ret2 = _spi->spi_put(FLASH_CE, NULL, NULL, 0);
	if (ret2 == 0)
		ret2 = _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, timeout);
	if (verbose) {
		if (ret2 == 0)
			printSuccess("DONE");
		else
			printError("FAIL");
	}

	if (!skip_bp_check && (bp != 0))
		ret = enable_protection(bp);

	return ret | ret2;
}

/* sector -> subsector for micron */
int SPIFlash::sector_erase(int addr)
{
	uint8_t tx[5];
	uint32_t len = 0;

	uint8_t cmd = (addr <= 0xffffff) ? FLASH_SE : FLASH_4SE;

	if (cmd == FLASH_4SE)
		tx[len++] = static_cast<uint8_t>(0xff & (addr >> 24));
	tx[len++] = static_cast<uint8_t>(0xff & (addr >> 16));
	tx[len++] = static_cast<uint8_t>(0xff & (addr >>  8));
	tx[len++] = static_cast<uint8_t>(0xff & (addr      ));

	_spi->spi_put(cmd, tx, NULL, len);

	return 0;
}

int SPIFlash::block32_erase(int addr)
{
	uint8_t tx[5];
	uint32_t len = 0;

	uint8_t cmd = (addr <= 0xffffff) ? FLASH_BE32 : FLASH_4BE32;

	if (cmd == FLASH_4BE32)
		tx[len++] = static_cast<uint8_t>(0xff & (addr >> 24));
	tx[len++] = static_cast<uint8_t>(0xff & (addr >> 16));
	tx[len++] = static_cast<uint8_t>(0xff & (addr >>  8));
	tx[len++] = static_cast<uint8_t>(0xff & (addr      ));

	_spi->spi_put(cmd, tx, NULL, len);

	return 0;
}

/* block64 -> sector for micron */
int SPIFlash::block64_erase(int addr)
{
	uint8_t tx[5];
	uint32_t len = 0;

	uint8_t cmd = (addr <= 0xffffff) ? FLASH_BE64 : FLASH_4BE64;

	if (cmd == FLASH_4BE64)
		tx[len++] = static_cast<uint8_t>(0xff & (addr >> 24));
	tx[len++] = static_cast<uint8_t>(0xff & (addr >> 16));
	tx[len++] = static_cast<uint8_t>(0xff & (addr >>  8));
	tx[len++] = static_cast<uint8_t>(0xff & (addr      ));

	_spi->spi_put(cmd, tx, NULL, len);

	return 0;
}

int SPIFlash::sectors_erase(int base_addr, int size)
{

	// check if chip support sector and subsector erase
	bool subsector_rdy = false, sector_rdy = true;
	if (_flash_model) {
		if (_flash_model->subsector_erase)
			subsector_rdy = true;
		if (!_flash_model->sector_erase)
			sector_rdy = false;
	}
	int ret = 0;
	int start_addr = base_addr;
	/* compute end_addr to be multiple of 4Kb */
	int end_addr = (base_addr + size + 0xfff) & ~0xfff;
	if (!subsector_rdy)
		end_addr = (base_addr + size + 0xffff) & ~0xffff;
	ProgressBar progress("Erasing", end_addr, 50, _verbose < 0);
	/* start with block size (64Kb) */
	int step = 0x10000;
	if (!sector_rdy)
		step = 0x1000;

	printf("start addr: %08x, end_addr: %08x\n", base_addr, (base_addr + size + 0xffff) & ~0xffff);

	for (int addr = start_addr; addr < end_addr; addr += step) {
		if (write_enable() == -1) {
			ret = -1;
			break;
		}

		/* if block erase + addr end out of end_addr -> use sector_erase (4Kb) */
		if (!sector_rdy || (addr + step > end_addr && subsector_rdy)) {
			step = 0x1000;
			ret = sector_erase(addr);
		} else {
			ret = block64_erase(addr);
		}

		if (ret == -1) {
			break;
		}
		if (_spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 100000, false) == -1) {
			ret = -1;
			break;
		}
		progress.display(addr);
	}
	if (ret == 0)
		progress.done();
	else
		progress.fail();

	return ret;
}

int SPIFlash::write_page(int addr, const uint8_t *data, int len)
{
	uint32_t addr_len;
	uint8_t write_cmd;
	uint32_t i = 0;

	if (addr <= 0xffffff) {
		addr_len = 3;
		write_cmd = FLASH_PP;
	} else {
		addr_len = 4;
		write_cmd = FLASH_4PP;
	}

	uint8_t tx[len+addr_len];

	if (write_cmd == FLASH_4PP)
		tx[i++] = (uint8_t)(0xff & (addr >> 24));
	tx[i++] = (uint8_t)(0xff & (addr >> 16));
	tx[i++] = (uint8_t)(0xff & (addr >>  8));
	tx[i++] = (uint8_t)(0xff & (addr      ));

	memcpy(tx+addr_len, data, len);

	if (write_enable() == -1)
		return -1;

	_spi->spi_put(write_cmd, tx, NULL, len+addr_len);
	return _spi->spi_wait(FLASH_RDSR, FLASH_RDSR_WIP, 0x00, 1000);
}