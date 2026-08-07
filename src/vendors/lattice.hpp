// SPDX-License-Identifier: Apache-2.0
/*
 * Copyright (C) 2019 Gwenhael Goavec-Merou <gwenhael.goavec-merou@trabucayre.com>
 */

#ifndef SRC_LATTICE_HPP_
#define SRC_LATTICE_HPP_

#include <stdint.h>
#include <stdio.h>
#include <iostream>
#include <list>
#include <string>
#include <vector>

#include "protocols/jtag.hpp"
#include "utils/device.hpp"
#include "parsers/jedParser.hpp"
#include "vendors/feaparser.hpp"
#include "vendors/latticeBitParser.hpp"
#include "protocols/flashInterface.hpp"

#define ISC_ENABLE					0xC6		/* ISC_ENABLE - Offline Mode */
#  define ISC_ENABLE_FLASH_MODE		(1 << 3)
#  define ISC_ENABLE_SRAM_MODE		(0 << 3)
#define ISC_ENABLE_TRANSPARENT		0x74		/* ISC_ENABLE_X This command is used to put the device in transparent mode */
#define LSC_BITSTREAM_BURST			0x7A        /* Program the device with the */
                                                /* bitstream sent in through the */
                                                /* JTAG port. */
#define ISC_DISABLE					0x26		/* ISC_DISABLE */
#define READ_DEVICE_ID_CODE			0xE0		/* IDCODE_PUB */
#define FLASH_ERASE					0x0E		/* ISC_ERASE */
/* Flash areas as defined for Lattice MachXO2,3L/LF */
#  define FLASH_ERASE_UFM			(1<<3)
#  define FLASH_ERASE_CFG   		(1<<2)
#  define FLASH_ERASE_FEATURE		(1<<1)
#  define FLASH_ERASE_SRAM			(1<<0)
#  define FLASH_ERASE_ALL       	0x0F
/* Flash areas as defined for Lattice MachXO3D, used with command: ISC_ERASE */
#  define FLASH_SEC_CFG0			(1<<8)
#  define FLASH_SEC_CFG1			(1<<9)
#  define FLASH_SEC_UFM0			(1<<10)
#  define FLASH_SEC_UFM1			(1<<11)
#  define FLASH_SEC_UFM2			(1<<12)
#  define FLASH_SEC_UFM3			(1<<13)
// #  define FLASH_SEC_CSEC			(1<<14)		/* not defined in later documentation */
// #  define FLASH_SEC_USEC			(1<<15)		/* not defined in later documentation */
#  define FLASH_SEC_PKEY			(1<<16)
#  define FLASH_SEC_AKEY			(1<<17)
#  define FLASH_SEC_FEA 			(1<<18)
#  define FLASH_SEC_ALL				0x7FF
/* This uses the same defines as above (for ISC_ERASE)
 * The Lattice Standard Doc for MachXO3D has incorrect list of operands for this
 * command.
 * This is document is more correct:
 *    fpga-tn-02119-1-1-using-hardened-control-functions-machxo3d-reference.pdf
 */
#define RESET_CFG_ADDR					0x46		/* LSC_INIT_ADDRESS */
/* Set the Page Address pointer to the Flash page specified */
#define LSC_WRITE_ADDRESS				0xB4
#  define FLASH_SET_ADDR_CFG0 			0x00
#  define FLASH_SET_ADDR_UFM0			0x01
#  define FLASH_SET_ADDR_FEA			0x03
#  define FLASH_SET_ADDR_CFG1			0x04
#  define FLASH_SET_ADDR_UFM1			0x05
#  define FLASH_SET_ADDR_PKEY			0x06
//#  define FLASH_SET_ADDR_CSEC			0x07		/* not defined in later documentation */
#  define FLASH_SET_ADDR_UFM2			0x08
#  define FLASH_SET_ADDR_UFM3 			0x09
#  define FLASH_SET_ADDR_AKEY			0x0A
//#  define FLASH_SET_ADDR_USEC			0x0B		/* not defined in later documentation */
/* Set the Page Address Pointer to the beginning of the UFM sectors.
 * It appears that this function is required when setting address to UFM sectors
 * The LSC_INIT_ADDRESS doesn't work when the sector is set to UFMx ... */
#define LSC_INIT_ADDR_UFM				0x47
#  define FLASH_UFM_ADDR_UFM0			(1<<10)
#  define FLASH_UFM_ADDR_UFM1			(1<<11)
#  define FLASH_UFM_ADDR_UFM2			(1<<12)
#  define FLASH_UFM_ADDR_UFM3			(1<<13)
#define PROG_CFG_FLASH					0x70		/* LSC_PROG_INCR_NV */
#define READ_BUSY_FLAG					0xF0		/* LSC_CHECK_BUSY */
#  define CHECK_BUSY_FLAG_BUSY          (1 << 7)
/* The busy flag defines bit 7 as busy, but busy flags returns 1 for busy (bit 0). */
#define REG_CFG_FLASH					0x73		/* LSC_READ_INCR_NV */
#define PROG_FEATURE_ROW				0xE4		/* LSC_PROG_FEATURE */
#define READ_FEATURE_ROW        		0xE7		/* LSC_READ_FEATURE */
/* See feaParser.hpp for FEATURE definitions */
#define PROG_FEABITS					0xF8		/* LSC_PROG_FEABITS */
#define READ_FEABITS            		0xFB		/* LSC_READ_FEABITS */
/* See feaParser.hpp for FEAbit definitions */
#define PROG_DONE						0x5E		/* ISC_PROGRAM_DONE - This command is used to program the done bit */
#define REFRESH							0x79		/* LSC_REFRESH - Equivalent to toggle PROGRAMN pin */
#define READ_STATUS_REGISTER    		0x3C		/* LSC_READ_STATUS */
#  define REG_STATUS_DONE				(1 << 8)	/* Flash or SRAM Done Flag (ISC_EN=0 -> 1 Successful Flash to SRAM transfer, ISC_EN=1 -> 1 Programmed) */
#  define REG_STATUS_ISC_EN				(1 << 9)	/* Enable Configuration Interface (1=Enable, 0=Disable) */
#  define REG_STATUS_BUSY				(1 << 12)	/* Busy Flag (1 = Busy) */
#  define REG_STATUS_FAIL				(1 << 13)	/* Fail Flag (1 = Operation failed) */
#  define REG_STATUS_PP_CFG				(1 << 15)	/* Password Protection All Enabled for CFG0 and CFG1 flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_PP_FSK				(1 << 16)	/* Password Protection Enabled for Feature and Security Key flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_PP_UFM				(1 << 17)	/* Password Protection enabled for all UFM flash sectors 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS_AUTH_DONE			(1 << 18)	/* Authentication done */
#  define REG_STATUS_PRI_BOOT_FAIL		(1 << 21)	/* Primary boot failure (1= Fail) even though secondary boot successful */
#  define REG_STATUS_CNF_CHK_MASK		(0x0f << 23)	/* Configuration Status Check */
# define REG_STATUS_PRV_CNF_CHK_MASK	(UINT64_C(0x0f) << 34)	/* NEXUS_FAMILY: Configuration Status Check of previous bitstrem */
#  define REG_STATUS_MACHXO3D_CNF_CHK_MASK	(0x0f << 22)	/* Configuration Status Check */
#  define REG_STATUS_EXEC_ERR			(1 << 26)	/*** NOT specified for MachXO3D ***/
#  define REG_STATUS_DEV_VERIFIED		(1 << 27)	/* I=0 Device verified correct, I=1 Device failed to verify */
#define READ_STATUS_REGISTER_1			0x3D        	/* LSC_READ_STATUS_1 */
#  define REG_STATUS1_FLASH_SEL			(0x0f << 0)	/* Flash sector selection 1=CFG0, 2=CFG1, 4=FEATURE, 5=Pub Key, 6=AES Key, 8=UFM0, 10=UFM1, 11=UFM2, 12=UFM3 */
#  define REG_STATUS1_ERASE_DISABLE		(1 << 4)	/* Erase operation is prohibited (1 = Erase disable) */
#  define REG_STATUS1_PROG_DISABLE		(1 << 5)	/* Program operation is prohibited (1 = Programing disable) */
#  define REG_STATUS1_READ_DISABLE		(1 << 6)	/* Read operation is prohibited (1 = Read disable) */
#  define REG_STATUS1_HS_LOCK_SEL		(1 << 7)	/* Hard/Soft Lock Selection (1 = Hard Lock, 0 = Soft Lock) */
#  define REG_STATUS1_AUTH_MODE			(0x03 << 8)	/* Authentication mode: 0x: No Authentication, 10: HMAC Authentication, 11: ECDSA Signature Verification */
#  define REG_STATUS1_AUTH_DONE_CFG0		(1 << 10)	/* Authentication done for CFG0 (1 = Authentication successful) */
#  define REG_STATUS1_AUTH_DONE_CFG1		(1 << 11)	/* Authentication done for CFG1 (1 = Authentication successful) */
#  define REG_STATUS1_FLASH_DONE_CFG0		(1 << 12)	/* Flash done bit is programmed of CFG0 (1= Programmed, 0=Unprogrammed) */
#  define REG_STATUS1_FLASH_DONE_CFG1		(1 << 13)	/* Flash done bit is programmed of CFG1 (1= Programmed, 0=Unprogrammed) */
#  define REG_STATUS1_SEC_PLUS_EN_CFG0		(1 << 14)	/* Security Plus enabled for CFG0 (1 = Enabled, 0 = Disabled) */
#  define REG_STATUS1_SEC_PLUS_EN_CFG1		(1 << 15)	/* Security Plus enabled for CFG1 (1 = Enabled, 0 = Disabled) */
#  define REG_STATUS1_BITSTR_VERSION		(1 << 16)	/* Bitstream version: 1 = Bitstream in CFG0 is latter (newer) than CFG1, 0 = Bitstream in CFG1 is latter (newer) than CFG0 */
#  define REG_STATUS1_BOOT_SEQ_SEL		(0x03 << 17)	/* Boot Sequence selection (used along with Master SPI Port Persistence bit) */
#  define REG_STATUS1_MSPI_PERS			(1 << 20)	/* Master SPI Port Persistence 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS1_I2C_DG_FILTER		(1 << 21)	/* I2C deglitch filter enable for Primary I2C Port 0=Disabled (Default), 1=Enabled */
#  define REG_STATUS1_I2C_DG_RANGE		(1 << 22)	/* I2C deglitch filter range selection on primary I2C port 0= 8 to 25 ns range (Default), 1= 16 to 50 ns range */
#define PROG_ECDSA_PUBKEY0				0x59		/* This command is used to program the first 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY0				0x5A		/* This command is used to read the first 128 bits of the ECDSA Public Key. */
#define PROG_ECDSA_PUBKEY1				0x5B		/* This command is used to program the second 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY1				0x5C		/* This command is used to read the second 128 bits of the ECDSA Public Key. */
#define PROG_ECDSA_PUBKEY2				0x61		/* This command is used to program the third 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY2				0x62		/* This command is used to read the third 128 bits of the ECDSA Public Key. */
#define PROG_ECDSA_PUBKEY3				0x63		/* This command is used to program the fourth 128 bits of the ECDSA Public Key. */
#define READ_ECDSA_PUBKEY3				0x64		/* This command is used to read the fourth 128 bits of the ECDSA Public Key. */
#define ISC_ENABLE_X					0x74
#define ISC_NOOP						0xff		/* This command is no operation command (NOOP) or null operation. */
#define LSC_DEVICE_CONTROL  0x7D    /* Multiple commands. Bit 3: configuration reset */
#define PRELOAD_SAMPLE      0x1C    /* PRELOAD/SAMPLE jtag opcode. Nexus family has Bscan register 362 bits-long => 45.25 => 46 bytes */
#define BYPASS              0xFF

#define PUBKEY_LENGTH_BYTES				64			/* length of the public key (MachXO3D) in bytes */

/* ECP3 */
#define ECP3_LSCC_BITSTREAM_BURST 0x02
#define ECP3_ISC_ERASE            0x03  /* ISC_ERASE */
#define ECP3_ISC_ENABLE           0x15
#define ECP3_IDCODE               0x16
#define ECP3_READ_USERCODE        0x17
#define ECP3_ISC_PROGRAM_USERCODE 0x1A
#define ECP3_RESET_ADDRESS        0x21
#define ECP3_LSCC_REFRESH         0x23
#define ECP3_READ_STATUS_REGISTER 0x53

/* Nexus */
#define REG_NEXUS_STATUS_BSE_ERR_MASK (0x0f << 24)
class Lattice: public Device, FlashInterface {
	public:
		Lattice(Jtag *jtag, std::string filename, const std::string &file_type,
			Device::prog_type_t prg_type, std::string flash_sector, bool verify,
			int8_t verbose, bool skip_load_bridge, bool skip_reset);
		uint32_t idCode() override;
		int userCode();
		bool write_userCode(uint32_t usercode);
		void reset() override;
		void program(unsigned int offset, bool unprotect_flash) override;
		bool program_mem();
		bool program_flash(unsigned int offset, bool unprotect_flash);
		bool Verify(std::vector<std::string> data, bool unlock = false,
				uint32_t flash_area = 0);
		bool dumpFlash(uint32_t base_addr, uint32_t len) override;

		/*!
		 * \brief display SPI flash ID and status register
		 */
		bool detect_flash() override {
			return FlashInterface::detect_flash();
		}
		/*!
		 * \brief protect SPI flash blocks
		 */
		bool protect_flash(uint32_t len) override {
			return FlashInterface::protect_flash(len);
		}
		/*!
		 * \brief protect SPI flash blocks
		 */
		bool unprotect_flash() override {
			return FlashInterface::unprotect_flash();
		}
		/*!
		 * \brief bulk erase SPI flash
		 */
		bool bulk_erase_flash() override;

		/* spi interface */
		int spi_put(uint8_t cmd, const uint8_t *tx, uint8_t *rx,
		uint32_t len) override;
		int spi_put(const uint8_t *tx, uint8_t *rx, uint32_t len) override;
		int spi_wait(uint8_t cmd, uint8_t mask, uint8_t cond,
				uint32_t timeout, bool verbose = false) override;

	private:
		enum lattice_family_t {
			MACHXO2_FAMILY = 0,
			MACHXO3_FAMILY = 1,
			MACHXO3D_FAMILY = 2,
			ECP5_FAMILY = 3,
			NEXUS_FAMILY = 4,
			ECP3_FAMILY,
			UNKNOWN_FAMILY = 999
		};

		lattice_family_t _fpga_family;

		/* Internal Registers structure */
		typedef struct {
			std::string description;
			uint8_t offset;
			uint8_t size;
			std::map<int, std::string> reg_cnt;
		} reg_struct_t;
		static const std::map<int, std::map<std::string, std::list<reg_struct_t>>> reg_content;

		int get_statusreg_size();

		bool program_intFlash(ConfigBitstreamParser *_cbp);
		bool dump_intFlash(uint32_t base_addr, uint32_t len);
		bool dump_intFlashPages(FILE *fd, const std::string &name,
				uint32_t area_base, uint32_t pages, uint32_t dump_base,
				uint32_t dump_len);
		void set_flash_sector(const std::string &flash_sector);
		bool program_extFlash(unsigned int offset, bool unprotect_flash);
		bool wr_rd(uint8_t cmd, uint8_t *tx, int tx_len,
				uint8_t *rx, int rx_len, bool verbose = false);
		/*!
		 * \brief move device to SPI access
		 */
		bool prepare_flash_access() override;
		/*!
		 * \brief end of device to SPI access
		 *        reload btistream from flash
		 */
		bool post_flash_access() override;
		/*!
		 * \brief erase SRAM
		 */
		bool clearSRAM();
		bool preload();
		void unlock();
		bool EnableISC(uint8_t flash_mode);
		bool DisableISC();
		bool EnableCfgIf();
		bool DisableCfg();
		bool pollBusyFlag(bool verbose = false);
		bool flashEraseAll();
		bool flashErase(uint32_t mask);
		bool flashProg(uint32_t start_addr, const std::string &name,
				std::vector<std::string> data);
		bool checkStatus(uint64_t val, uint64_t mask);
		void displayReadReg(uint64_t dev);
		uint64_t readStatusReg();
		uint64_t readFeaturesRow();
		bool writeFeaturesRow(uint64_t features, bool verify);
		uint16_t readFeabits();
		bool writeFeabits(uint16_t feabits, bool verify);
		bool writeProgramDone();
		bool loadConfiguration();
		uint16_t getUFMStartPageFromJEDEC(JedParser *_jed, int id);

		/* test */
		bool checkID();

		/************************* MODS for ECP3 ******************************/
		void usleep_ecp3(uint64_t us_time);

		/*********************** MODS FOR MacXO3D *****************************/
		enum lattice_flash_sector_t {
			LATTICE_FLASH_UNDEFINED = 0,
			LATTICE_FLASH_CFG,
			LATTICE_FLASH_UFM,
			LATTICE_FLASH_FEATURE,
			LATTICE_FLASH_SRAM,
			LATTICE_FLASH_ALL,
			LATTICE_FLASH_CFG0,
			LATTICE_FLASH_CFG1,
			LATTICE_FLASH_UFM0,
			LATTICE_FLASH_UFM1,
			LATTICE_FLASH_UFM2,
			LATTICE_FLASH_UFM3,
			LATTICE_FLASH_FEA,
			LATTICE_FLASH_PKEY,
			LATTICE_FLASH_AKEY,
			LATTICE_FLASH_CSEC,
			LATTICE_FLASH_USEC
		};

		lattice_flash_sector_t _flash_sector;
		bool programFeatureRow_MachXO3D(uint8_t* feature_row);
		bool programFeabits_MachXO3D(uint32_t feabits);
		bool programPubKey_MachXO3D(uint8_t* pubkey);

		bool program_intFlash_MachXO3D(JedParser& _jed);
		bool program_fea_MachXO3D();
		bool program_pubkey_MachXO3D();
};
#endif  // SRC_LATTICE_HPP_
