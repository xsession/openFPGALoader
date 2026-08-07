// SPI Flash dump operations
#include "protocols/spiFlash.hpp"
#include <cstring>
#include "utils/display.hpp"
#include "utils/progressBar.hpp"

bool SPIFlash::dump(const std::string &filename, const int &base_addr,
		const int &len, int rd_burst, const std::string &dump_format)
{
	int dump_len = len;
	if (dump_len == 0) {
		const uint32_t flash_capacity = capacity();
		if (flash_capacity == 0) {
			printError("Error: --file-size is required for unknown SPI flash chips");
			return false;
		}
		if (base_addr < 0 || static_cast<uint32_t>(base_addr) >= flash_capacity) {
			printError("Error: dump offset is outside SPI flash capacity");
			return false;
		}
		const uint32_t remaining = flash_capacity - base_addr;
		if (remaining > static_cast<uint32_t>(std::numeric_limits<int>::max())) {
			printError("Error: auto dump size is too large for this build; use --file-size");
			return false;
		}
		dump_len = remaining;
		char content[128];
		snprintf(content, sizeof(content),
			"Auto dump size: %d bytes (flash capacity %u bytes, offset %d)",
			dump_len, flash_capacity, base_addr);
		printInfo(content);
	}

	if (dump_len <= 0) {
		printError("Error: invalid dump size");
		return false;
	}

	if (rd_burst == 0)
		rd_burst = dump_len;

	/* segfault with buffer > 1M */
	if (rd_burst > 0x100000)
		rd_burst = 0x100000;

	/* Accumulate all data in memory */
	std::string buffer;
	buffer.resize(dump_len);

	printInfo("dump flash (May take time)");

	ProgressBar progress("Read flash ", dump_len, 50, _verbose < 0);
	for (int i = 0; i < dump_len; i += rd_burst) {
		if (rd_burst + i > dump_len)
			rd_burst = dump_len - i;
		if (0 != read(base_addr + i, (uint8_t*)&buffer[i], rd_burst)) {
			progress.fail();
			printError("Failed to read flash");
			return false;
		}
		progress.display(i);
	}
	progress.done();

	printInfo("Open dump file ", false);
	FILE *fd = fopen(filename.c_str(), "wb");
	if (!fd) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	printInfo("Write flash ", false);

	if (dump_format == "mcs") {
		/* Write Intel HEX format */
		size_t offset = 0;
		const uint8_t *data = reinterpret_cast<const uint8_t *>(buffer.c_str());

		/* Emit Extended Linear Address records at every 64 KB boundary,
		 * matching Xilinx Impact behavior. Each data segment starts at
		 * address 0x0000 within the segment base set by the ELA record. */
		fprintf(fd, ":020000040000FA\n");

		while (offset < buffer.size()) {
			size_t remaining = buffer.size() - offset;
			uint8_t count = static_cast<uint8_t>(std::min(remaining, (size_t)16));
			uint16_t addr = static_cast<uint16_t>(offset);

			uint8_t checksum = 0;
			checksum = (checksum + count) & 0xff;
			checksum = (checksum + (addr >> 8)) & 0xff;
			checksum = (checksum + (addr & 0xff)) & 0xff;
			checksum = (checksum + 0) & 0xff; /* type 00 = data */
			for (uint8_t j = 0; j < count; j++) {
				checksum = (checksum + data[offset + j]) & 0xff;
			}
			checksum = (~checksum) + 1;

			fprintf(fd, ":%02X%04X00", count, addr);
			for (uint8_t j = 0; j < count; j++) {
				fprintf(fd, "%02X", data[offset + j]);
			}
			fprintf(fd, "%02X\n", checksum);

			offset += count;

			if (offset < buffer.size() && (offset & 0xFFFF) == 0) {
				uint16_t segment = static_cast<uint16_t>(offset >> 16);
				uint8_t cs = 0;
				cs = (cs + 2) & 0xff;
				cs = (cs + 0) & 0xff;
				cs = (cs + 0) & 0xff;
				cs = (cs + 4) & 0xff;
				cs = (cs + (segment >> 8)) & 0xff;
				cs = (cs + (segment & 0xff)) & 0xff;
				cs = (~cs) + 1;
				fprintf(fd, ":02000004%04X%02X\n", segment, cs);
			}
		}
		fprintf(fd, ":00000001FF\n");
	} else {
		/* Raw binary format */
		fwrite(buffer.c_str(), sizeof(uint8_t), buffer.size(), fd);
	}

	printSuccess("DONE");

	if (fclose(fd) != 0) {
		printError("Failed to close dump file: " + std::string(strerror(errno)));
		remove(filename.c_str());
		return false;
	}

	return true;
}

uint32_t SPIFlash::capacity() const
{
	if (!_flash_model)
		return 0;
	return _flash_model->nr_sector * 0x10000;
}

bool SPIFlash::prepare_flash(const int base_addr, const int len)
{
	/* If flash not already detected: do that here */
	if (_jedec_id == 0) {
		try {
			read_id();
		} catch(std::exception &e) {
			printError(e.what());
			return false;
		}
	}

	/* check Block Protect Bits (hide WIP/WEN bits) */
	_status = read_status_reg() & ~0x03;
	if (_verbose > 0)
		display_status_reg(_status);

	/* if known chip */
	if (_flash_model) {
		if (_flash_model->nr_sector == 0) {
			printError("Error: SPI flash capacity is unknown; erase/program is disabled for this RDID");
			return false;
		}
		/* microchip SST26VF032B/64B have global lock set
		 * at powerup. global unlock must be send unconditionally
		 * with or without block protection
		 */
		if (_flash_model->global_lock) {
			if (!global_unlock())
				return false;
		}

		/* check if offset + len fit in flash */
		if ((unsigned int)(base_addr + len) > (_flash_model->nr_sector * 0x10000)) {
			printError("flash overflow");
			return false;
		}
		// if device has block protect
		if (_flash_model->bp_len != 0) {
			/* compute protected area */
			int8_t tb = get_tb();
			if (tb == -1)
				return false;
			std::map<std::string, uint32_t> lock_len = bp_to_len(_status, tb);
			printf("%08x %08x %08x %02x\n", base_addr,
					lock_len["start"], lock_len["end"], _status);

			/* if some blocks are locked */
			if (lock_len["start"] != 0 || lock_len["end"] != 0) {
				/* if overlap */
				if (tb == 1) {  // bottom blocks are protected
								// check if start is in protected blocks
					if ((uint32_t)base_addr <= lock_len["end"])
						_must_relock = true;
				} else {  // top blocks
					if ((uint32_t)(base_addr + len) >= lock_len["start"])
						_must_relock = true;
				}
			}
			const uint32_t jedec = _jedec_id >> 8;
			/* ISSI IS25LP032 seems have a bug:
			 * block protection is always in top mode regardless of
			 * the TB bit: if write is not at offset 0 -> force unlock
			 */
			if (jedec == 0x9d6016 && tb == 1 && base_addr != 0) {
				_unprotect = true;
				_must_relock = true;
			}
			/* ST M25P40 / M25P16 / MX25L6045 have not TB bit:
			 * block protection is always in top mode:
			 * if write is not at offset 0 -> force unlock
			 */
			if (((jedec == 0x202013) || (jedec == 0x202015) ||
				(jedec == 0xC22017))
				&& tb == 1 && base_addr != 0) {
				_unprotect = true;
				_must_relock = true;
			}
		}
	} else {  // unknown chip: basic test
		printWarn("flash chip unknown: use basic protection detection");
		if (get_bp() != 0)
			_must_relock = true;
	}

	/* if it's needs to unlock... */
	/* Checks if unlock is asked/allowed by the user */
	if (_must_relock) {
		printf("unlock blocks\n");
		if (!_unprotect) {
			printError("Error: block protection is set");
			printError("       can't unlock without --unprotect-flash");
			return false;
		} else  {
			if (disable_protection() != 0)
				return false;
		}
	}

	return true;
}