// Lattice internal flash programming
#include "vendors/lattice.hpp"
#include <algorithm>

bool Lattice::program_intFlash(ConfigBitstreamParser *_cbp)
{
	uint64_t featuresRow;
	uint16_t ufm_start = 0;
	uint16_t feabits;
	uint8_t eraseMode = 0;
	std::vector<std::string> ufm_data, cfg_data, ebr_data;
	auto all_zero = [](const std::vector<std::string> &data) {
		return std::all_of(data.begin(), data.end(),
			[](const std::string &line) {
				return std::all_of(line.begin(), line.end(),
					[](char value) { return value == 0; });
			});
	};

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	/* ISC Enable 0xC6 followed by
	 * 0x08 (Enable nVCM/Flash Normal mode */
	printInfo("Enable configuration: ", false);
	if (!EnableISC(0x08)) {
		printError("FAIL");
		displayReadReg(readStatusReg());
		return false;
	} else {
		printSuccess("DONE");
	}

	//  If file is a jed -> classic approach for
	//  all machXO
	if (_file_extension == "jed") {
		JedParser *_jed = reinterpret_cast<JedParser *>(_cbp);
		uint16_t max_ufm_pages = 2046;
		const std::string model = fpga_list[_jtag->get_target_device_id()].model;
		if (model.find("9400") != std::string::npos)
			max_ufm_pages = 3582;
		else if (model.find("4300") != std::string::npos)
			max_ufm_pages = 767;
		else if (model.find("2100") != std::string::npos)
			max_ufm_pages = 639;
		else if (model.find("1300") != std::string::npos)
			max_ufm_pages = 511;
		else if (model.find("640") != std::string::npos)
			max_ufm_pages = 191;

		for (size_t i = 0; i < _jed->nb_section(); i++) {
			std::string note = _jed->noteForSection(i);
			if (note == "TAG DATA") {
				eraseMode |= FLASH_ERASE_UFM;
				ufm_data = _jed->data_for_section(i);
				ufm_start = getUFMStartPageFromJEDEC(_jed, i);

				if (_verbose)
					printInfo("UFM init detected in JEDEC file");

				if(ufm_start >= max_ufm_pages) {
					if (all_zero(ufm_data)) {
						if (_verbose)
							printInfo("Skipping empty UFM TAG DATA section");
						eraseMode &= ~FLASH_ERASE_UFM;
						continue;
					}
					printError("UFM section detected in JEDEC file, but "
						"calculated flash start address was out of bounds");
					return false;
				}
			} else if (note == "END CONFIG DATA") {
				continue;
			} else if (note == "EBR_INIT DATA") {
				ebr_data = _jed->data_for_section(i);
			} else {
				cfg_data = _jed->data_for_section(i);
			}
		}

		/* check if feature area must be updated */
		featuresRow = _jed->featuresRow();
		feabits = _jed->feabits();
	} else {  // bit file: adapts
		LatticeBitParser *_bit = reinterpret_cast<LatticeBitParser *>(_cbp);
		cfg_data = _bit->getDataArray();
		featuresRow = 0;
		feabits = 0x460;
	}

	eraseMode |= FLASH_ERASE_CFG;
	if (featuresRow != readFeaturesRow() || feabits != readFeabits())
		eraseMode |= FLASH_ERASE_FEATURE;

	/* ISC ERASE */
	printInfo("Flash erase: ", false);
	if (flashErase(eraseMode) == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* LSC_INIT_ADDRESS */
	wr_rd(0x46, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	/* flash CfgFlash */
	if (false == flashProg(0, "data", cfg_data))
		return false;

	/* flash EBR Init */
	if (ebr_data.size()) {
		if (false == flashProg(0, "EBR", ebr_data))
			return false;
	}
	/* verify write */
	if (_verify) {
		if (Verify(cfg_data) == false)
			return false;
	}

	if ((eraseMode & FLASH_ERASE_UFM) != 0) {
		/* LSC_WRITE_ADDRESS */
		uint8_t tx[4] = {
			static_cast<uint8_t>(ufm_start & 0xff),
			static_cast<uint8_t>((ufm_start >> 8) & 0xff),
			0,
			0x40
		};

		wr_rd(LSC_WRITE_ADDRESS, tx, 4, NULL, 0);
		_jtag->set_state(Jtag::RUN_TEST_IDLE);
		_jtag->toggleClk(1000);

		/* Same command to program CFG flash works for UFM. */
		if (false == flashProg(0, "UFM", ufm_data))
			return false;
	}

	/* missing usercode update */

	/* LSC_INIT_ADDRESS */
	wr_rd(0x46, NULL, 0, NULL, 0);
	_jtag->set_state(Jtag::RUN_TEST_IDLE);
	_jtag->toggleClk(1000);

	if ((eraseMode & FLASH_ERASE_FEATURE) != 0) {
		/* write feature row */
		printInfo("Program features Row: ", false);
		if (writeFeaturesRow(featuresRow, true) == false) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
		/* write feabits */
		printInfo("Program feabits: ", false);
		if (writeFeabits(feabits, true) == false) {
			printError("FAIL");
			return false;
		} else {
			printSuccess("DONE");
		}
	}

	/* ISC program done 0x5E */
	printInfo("Write program Done: ", false);
	if (writeProgramDone() == false) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}

	/* bypass */
	wr_rd(0xff, NULL, 0, NULL, 0);
	/* disable configuration mode */
	printInfo("Disable configuration: ", false);
	if (!DisableISC()) {
		printError("FAIL");
		return false;
	} else {
		printSuccess("DONE");
	}
	return true;
}

