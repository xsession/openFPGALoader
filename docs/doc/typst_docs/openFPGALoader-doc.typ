// openFPGALoader Documentation -- Typst source
// Compile with: typst compile openFPGALoader-doc.typ --pdf

#set(page(paper: "a4", margin: 20pt, fill: white))
#set(text(font: "Source Sans 3 Pro", size: 10.5pt, fill: rgb("1a1a1a")))
#set(heading(numbering: "1.1"))
#set(heading(1, fill: rgb("1a3a5c")))
#set(par(leading: 14pt))

== openFPGALoader v1.1.2 -- Technical Reference Manual ==

*Universal FPGA Programming Utility -- Fork with Windows, XPCU, and Enhanced Flash Support*

---

=== Table of Contents ===

#toc()

---

=== 1. Introduction ===

openFPGALoader is an open-source, cross-platform CLI utility for programming and configuring Field-Programmable Gate Arrays (FPGAs) through JTAG, SPI, and DFU interfaces. It supports a wide range of FPGA vendors, cable/adapter types, and board configurations.

This document describes version *1.1.2* of the *xsession fork* of the upstream `trabucayre/openFPGALoader` project, which adds significant enhancements in Windows cross-compilation, Xilinx Platform Cable USB (XPCU) support, external SPI flash handling, and Lattice internal Flash/NVCM operations.

==== 1.1 License and Attribution ====

- *License:* Apache 2.0 (SPDX: `Apache-2.0`)
- *Original Author:* Gwenhael Goavec-Merou, gwenhael.goavec-merou@trabucayre.com
- *Fork baseline:* Upstream merge base `d52abf70` on `https://github.com/trabucayre/openFPGALoader.git`
- *IRC Channel:* `#openFPGALoader` on libera.chat

==== 1.2 Build System ====

openFPGALoader uses *CMake 3.10+* with modular compile-time options. The project is written in *C++17*.

- *Core build:* `cmake -B build && cmake --build build`
- *Static build:* `cmake -B build -DBUILD_STATIC=ON`
- *Windows cross-compile:* Docker-based MinGW64 pipeline
- *Linux Docker deploy:* Alpine-based packaging pipeline

==== 1.3 Key Compile-Time Toggles ====

#grid(
  columns: (1fr, 0.8fr, 3fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  align: horizon,
  [#lozenge("Option", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Default", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Description", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [`ENABLE_CABLE_ALL`], [ON], [Enable all cable drivers],
  [`ENABLE_VENDORS_ALL`], [ON], [Enable all FPGA vendor drivers],
  [`BUILD_STATIC`], [OFF], [Link static libraries],
  [`ENABLE_OPTIM`], [ON], [-O3 optimization level],
  [`ENABLE_LIBFTDI`], [auto], [libftdi-based cables (FTDI, USB Blaster I)],
  [`ENABLE_LIBUSB`], [auto], [libusb-based cables (J-Link, DirtyJTAG, etc.)],
  [`ENABLE_UDEV`], [ON], [Linux udev device auto-detection],
  [`ENABLE_LIBGPIOD`], [auto], [Linux GPIO bitbang driver],
  [`ENABLE_CMSISDAP`], [ON], [CMSIS-DAP v1 (hidapi) + v2 (libusb)],
  [`ENABLE_DFU`], [ON], [DFU mode for ECP5/iCE40 boards],
  [`ENABLE_SVF_JTAG`], [ON], [SVF JTAG script support],
  [`ENABLE_XVC_CLIENT`], [ON], [Xilinx Virtual Cable client (non-Windows)],
  [`ENABLE_XPCU`], [ON], [Xilinx Platform Cable USB (fork feature)],
)

---

=== 2. Architecture ===

openFPGALoader follows a layered architecture with clear separation between the hardware abstraction layer, vendor-specific programming logic, and file format parsers.

==== 2.1 Core Classes ====

- *main.cpp* -- CLI entry point using `cxxopts` for argument parsing. Orchestrates cable detection, JTAG chain scanning, device instantiation, and programming flow.

- *`Device` (abstract base)* -- `src/device.hpp`: Defines the virtual interface for all FPGA vendors: `program()`, `dumpFlash()`, `protect_flash()` / `unprotect_flash()`, `bulk_erase_flash()`, `idCode()`, `reset()`.

- *`Jtag`* -- `src/jtag.hpp`: Manages the JTAG TAP controller chain. Chain detection via IDCODE scan, TCK frequency control (default 6 MHz), IR/DR operation selection by index, read/write edge configuration.

- *`Cable`* -- `src/cable.hpp`: Defines `communication_type` enum and `cable_t` structure mapping VID/PID pairs to driver backends. 18+ communication modes supported.

- *`Board`* -- `src/board.hpp`: Maps board names to cable type, FPGA part, pin configuration, and communication mode. 80+ predefined boards.

- *`SPIFlash`* -- `src/spiFlash.hpp`: SPI flash memory abstraction with support for sector erase, subsector erase, protection bits, Quad mode, power management, and flash data section handling with gap-aware writing.

- *`FlashInterface`* (abstract mixin) -- Provides `spi_put()`, `spi_wait()`, `spi_read()` virtual methods. Vendor classes inherit this to route SPI commands over JTAG bridges or direct SPI.

==== 2.2 Vendor Implementations ====

#grid(
  columns: (1.5fr, 1.5fr, 2.5fr, 2fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Vendor", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Source", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Families", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("File Formats", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [Xilinx], [`xilinx.cpp`], [XC2C, XC9500XL, Virtex4/6/7, Spartan3/6/7, Artix7, Kintex7, Zynq, UltraScale/+, ZynqMP, XCF], [`.bit`, `.bit.gz`, `.jed`, `.mcs`],
  [Altera/Intel], [`altera.cpp`], [Max II, Cyclone II/III/IV/V, 10LP, MAX 10, Stratix V, Arria 10, Agilex 3/5], [`.raw`, `.pof`, `.svf`],
  [Lattice], [`lattice.cpp`], [XP2, MachXO2/3/3L/3LF/3D, ECP3/5, CrossLink-NX, Certus-NX, CertusPro-NX], [`.bit`, `.jed`, `.fea`],
  [Gowin], [`gowin.cpp`], [GW1N/R/NS/NSR/NZ, GW2A, GW5A/AST], [`.fs`, `.fs.gz`],
  [Anlogic], [`anlogic.cpp`], [Eagle D20/S20, Elf2 EF2M45], [`.bit`],
  [Efinix], [`efinix.cpp`], [Trion T4-T120, Titanium Ti35-Ti375], [`.hex`],
  [CologneChip], [`colognechip.cpp`], [GateMate GM1Ax], [`.cfg`],
  [iCE40], [`ice40.cpp`], [HX1K, UP5K], [via SSPI over FTDI],
)

==== 2.3 Cable/Adapter Drivers ====

#grid(
  columns: (1.5fr, 1.5fr, 1.5fr, 2.5fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Cable", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Mode", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Source", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Notes", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [ft2232, ft4232, ft232, etc.], [MODE_FTDI_SERIAL], [`ftdiJtagMPSSE.cpp`], [FTDI MPSSE engine, multiple board pinouts],
  [ft232RL, ft231X], [MODE_FTDI_BITBANG], [`ftdiJtagBitbang.cpp`], [Bitbang mode for single-chip FTDI],
  [jlink], [MODE_JLINK], [`jlink.cpp`], [Segger J-Link via libusb],
  [dirtyJtag], [MODE_DIRTYJTAG], [`dirtyJtag.cpp`], [STM32F1-based open-hardware JTAG],
  [usb-blaster], [MODE_USBBLASTER], [`usbBlaster.cpp`], [Altera USB Blaster I (FX2)],
  [usb-blasterII/III], [MODE_FX2_LL], [`fx2_ll.cpp`], [Altera USB Blaster II/III with firmware upload],
  [cmsisdap], [MODE_CMSISDAP], [`cmsisDAP.cpp`], [CMSIS-DAP v1 (HIDAPI) + v2 (libusb)],
  [dfu], [MODE_DFU], [`dfu.cpp`], [DFU mode for ECP5/iCE40],
  [xpcu], [MODE_XPCU], [`xilinxPlatformCableUSB.cpp`], [Xilinx Platform Cable USB -- fork feature],
  [ch552_jtag], [MODE_CH552_JTAG], [`ch552_jtag.cpp`], [CH552-based JTAG (TangNano)],
  [ch347], [MODE_CH347], [`ch347jtag.cpp`], [CH347 JTAG adapter],
  [gwu2x], [MODE_GWU2X], [`gwu2x_jtag.cpp`], [Gowin GWU2X programmer],
  [esp32s3], [MODE_ESP], [`esp_usb_jtag.cpp`], [ESP32-S3/ESP32-C3 USB JTAG],
  [anlogicCable], [MODE_ANLOGICCABLE], [`anlogicCable.cpp`], [Anlogic proprietary cable],
  [xvc-client], [MODE_XVC_CLIENT], [`xvc_client.cpp`], [Xilinx Virtual Cable TCP client],
  [xvc-server], [MODE_XVC_SERVER], [`xvc_server.cpp`], [Xilinx Virtual Cable TCP server],
  [libgpiod_bitbang], [MODE_LIBGPIOD_BITBANG], [`libgpiodJtagBitbang.cpp`], [Linux libgpiod GPIO bitbang],
  [remoteBitbang], [MODE_REMOTEBITBANG], [`remoteBitbang_client.cpp`], [Remote Bitbang TCP client],
)

==== 2.4 File Format Parsers ====

#grid(
  columns: (1.5fr, 2fr, 1.5fr, 2fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Parser", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Source", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Format", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Used By", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [ConfigBitstreamParser], [`configBitstreamParser.cpp`], [`.bit` / `.bit.gz`], [Xilinx],
  [JedParser], [`jedParser.cpp`], [`.jed` (JEDec)], [Xilinx, Lattice],
  [McsParser], [`mcsParser.cpp`], [`.mcs`], [Xilinx, Lattice, Altera],
  [PofParser], [`pofParser.cpp`], [`.pof`], [Altera],
  [LatticeBitParser], [`latticeBitParser.cpp`], [`.bit` (Lattice)], [Lattice MachXO/ECP5],
  [FeaParser], [`feaParser.cpp`], [`.fea`], [Lattice],
  [FsParser], [`fsparser.cpp`], [`.fs` / `.fs.gz`], [Gowin],
  [AnlogicBitParser], [`anlogicBitParser.cpp`], [`.bit` (Anlogic)], [Anlogic],
  [EfinixHexParser], [`efinixHexParser.cpp`], [`.hex`], [Efinix],
  [ColognechipCfgParser], [`colognechipCfgParser.cpp`], [`.cfg`], [CologneChip],
  [RawParser], [`rawParser.cpp`], [`.raw` / `.bin`], [Altera, generic],
  [DfuFileParser], [`dfuFileParser.cpp`], [`.dfu`], [DFU devices],
  [IhexParser], [`ihexParser.cpp`], [Intel HEX], [XPCU firmware],
)

---

=== 3. Supported Hardware ===

==== 3.1 FPGA Devices ====

The device database (`src/part.hpp`) maps 32-bit JTAG IDCODE values to vendor, family, model, and IR length.

**Xilinx (100+ devices):** XC2C, XC9500XL CPLDs; Virtex4, Virtex6, Virtex7 (6-38 bit IR); Spartan3/E, Spartan6, Spartan7; Artix7, Kintex7, Zynq 7000; UltraScale (KintexU, VirtexU); UltraScale+ (ArtixUSP, KintexUSP, VirtexUSP, SpartanUSP); ZynqMP (xczu2cg through xczu49dr) -- requires `JTAG_CTRL` register write for PL/ARM discovery; XCF configuration devices.

**Altera/Intel (30+ devices):** Max II; Cyclone II, III, IV/IV GX, 10 LP, V, V SoC; MAX 10 (single-supply and dual-supply); Stratix V GS; Arria 10 (GX, GT, SX SoC variants); Agilex 3 (A3CY100, A3CZ135); Agilex 5 (A5EC008B).

**Lattice (40+ devices):** XP2 (LFXP2-8E); MachXO2 (256-7000, ZE/HC/UHC); MachXO3L (640-9400, E/C) -- fork adds internal Flash/NVCM dump/erase; MachXO3LF (640-9400, E/C) -- fork adds internal Flash/NVCM dump/erase; MachXO3D (9400HC); ECP3 (70E, 150EA); ECP5 (U-12/25/45/85, UM-25/45/85, UM5G-25/45/85); CrossLink-NX (LIFCL-17/40); Certus-NX (LFD2NX-17/40); CertusPro-NX (LFCPNX-100).

**Gowin (12+ devices):** GW1N/R/NS/NSR/NZ (1-9C); GW2A/R (18C, 55); GW5A/AST (15-138).

**Efinix (7 devices):** Trion: T4/T8, T8QFP144/T13/T20, T55/T85/T120, T20BGA324/T35; Titanium: Ti60, Ti60ES, Ti35, Ti180, Ti375.

**Anlogic (3 devices):** Eagle D20 (EG4D20EG176), S20 (EG4S20BG256); Elf2 (EF2M45).

**CologneChip:** GateMate GM1Ax.

==== 3.2 SPI Flash Database ====

The flash database (`src/spiFlashdb.hpp`) catalogs 40+ SPI NOR flash chips with their JEDEC IDs, sector layouts, erase capabilities, protection bit mappings, and Quad Enable settings.

#grid(
  columns: (1.5fr, 2fr, 1.5fr, 1fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Manufacturer", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Models", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("JEDEC ID(s)", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Capacity", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [Macronix], [MX77L25650F], [0xc27519, 0x77b80a], [32 MiB],
  [Spansion/Infineon], [S25FL064P, S25FL128S, S25FL256S, S25FL512S, S25FL128L, S25FL256L], [0x01xxxx], [8-64 MiB],
  [ST/Micron], [M25P40, M25P80, M25P16, M25P32], [0x202013-16], [512 KiB-4 MiB],
  [Micron], [N25Q32, N25Q64, N25Q128, N25Q256, MT25/N25Q128, MT25QU512, MT25QU01G, MT25QU02G], [0x20ba16-19, 0x20bb18-22], [4-256 MiB],
  [Xilinx], [XCF32P], [0x05059093], [4 MiB],
  [XTX], [XT25F32B-S], [0x0b4016], [4 MiB],
  [PUYA], [P25Q32H], [0x856016], [4 MiB],
  [ISSI], [IS25LP032, IS25LP064, IS25LP128], [0x9d6016-18], [4-16 MiB],
)

==== 3.3 Predefined Boards ====

Over 80 development boards are predefined in `src/board.hpp`, including:

#grid(
  columns: (1.8fr, 2fr, 1fr, 0.8fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Board", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("FPGA Part", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Cable", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Flash", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [arty_a7_35t], [xc7a35tcsg324], [digilent], [SPI],
  [arty_a7_100t], [xc7a100tcsg324], [digilent], [SPI],
  [basys3], [xc7a35tcpg236], [digilent], [SPI],
  [kc705], [xc7k325tffg900], [digilent], [SPI],
  [zc702, zc706], [xc7z020/045], [digilent/jtag-smt2-nc], [SPI],
  [zedboard], [xc7z020clg484], [digilent_hs2], [SPI],
  [zybo_z7_10/20], [xc7z010/020], [digilent], [SPI],
  [de0nano], [ep4ce2217], [usb-blaster], [SPI],
  [de10nano], [5CSEBA6U23I7], [usb-blasterII], [SPI],
  [de1Soc], [5CSEMA5], [usb-blasterII], [SPI],
  [tangnano1k/4k/9k/20k], [GW1N/R], [ft2232/ch552_jtag], [SPI],
  [fomu], [iCE40 UP5K], [dfu], [SPI],
  [icebreaker-bitsy], [iCE40 UP5K], [dfu], [SPI],
  [ecpix5], [ECP5-85F], [ecpix5-debug], [SPI],
  [ulx3s], [ECP5], [ft231X], [SPI],
  [licheeTang], [EG4/Eagle], [anlogicCable], [SPI],
  [colorlight-i5/i9], [ECP5], [cmsisdap], [SPI],
  [pynq_z1/z2], [xc7z020], [ft2232], [SPI],
)

---

=== 4. Usage Reference ===

==== 4.1 Command-Line Syntax ====

`openFPGALoader [OPTIONS] BIT_FILE`

==== 4.2 Core Options ====

#grid(
  columns: (2.5fr, 4.5fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Option", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Description", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [`-c, --cable <name>`], [JTAG interface cable name (see cable list)],
  [`-b, --board <name>`], [Board name (implies cable + FPGA part)],
  [`-m, --write-sram`], [Write bitstream to FPGA SRAM (default)],
  [`-f, --write-flash`], [Write bitstream to external flash],
  [`--dump-flash`], [Dump flash contents to file],
  [`--bulk-erase`], [Bulk erase flash memory],
  [`-r, --reset`], [Reset FPGA after programming],
  [`-v, --verbose`], [Verbose output (repeat for more detail)],
  [`--detect`], [Detect FPGA and connected flash],
  [`--list-cables`], [List all supported cables],
  [`--list-boards`], [List all supported boards],
  [`--list-fpga`], [List all supported FPGA devices],
  [`--list-flash`], [List all supported SPI flash chips],
)

==== 4.3 Advanced Options ====

#grid(
  columns: (3fr, 4fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Option", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Description", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [`--freq <Hz>`], [JTAG clock frequency in Hz (default: 6 MHz)],
  [`-o, --offset <bytes>`], [Start address for flash read/write],
  [`--file-size <bytes>`], [Size to dump (0 or omitted = full flash)],
  [`--file-type <type>`], [Force file type instead of auto-detection],
  [`--fpga-part <part>`], [Specify FPGA model flavor + package],
  [`--index-chain <n>`], [Device index in JTAG chain],
  [`--vid / --pid`], [Probe Vendor/Product ID],
  [`--usb-serial-num <ser>`], [USB serial number (FTDI or ESP32)],
  [`--busdev-num <b:d>`], [Select probe by bus:device number],
  [`--cable-index <n>`], [Probe index (FTDI and CMSIS-DAP)],
  [`--ftdi-channel <n>`], [FTDI channel 0-3 (A-D)],
  [`--secondary-bitstream <file>`], [Secondary bitstream (UltraScale boards)],
  [`--target-flash <primary|secondary|both>`], [Select target flash chip],
  [`--enable-quad / --disable-quad`], [Enable/disable SPI Quad mode],
  [`--protect-flash <len>`], [Protect SPI flash area],
  [`--unprotect-flash`], [Unprotect flash blocks],
  [`--verify`], [Verify flash write (SPI only)],
)

==== 4.4 Fork-Specific Options ====

#grid(
  columns: (3fr, 4fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Option", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Description", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [`--probe-firmware <file>`], [Firmware for JTAG probe (usbBlasterII/XPCU)],
  [`--skip-probe-firmware-upload`], [Skip FX2 firmware upload (XPCU: open initialized 03fd:0008 directly)],
  [`--xpcu-direct-xp2-firmware`], [For cold PID 03fd:0013, upload xusb_xp2.hex directly],
  [`--external-flash-type <id>`], [Force SPI flash type by JEDEC ID or model name (e.g., M25P40)],
  [`--detect-external-flash`], [Detect and display external SPI flash chip info],
  [`--flash-sector <sector>`], [Flash sector for Lattice (CFG/UFM/FEATURE/SRAM/ALL) and Altera MAX10],
  [`--skip-load-bridge`], [Skip writing bridge bitstream to SRAM in write-flash mode],
  [`--skip-reset`], [Skip device reset in write-flash mode],
  [`--bridge <path>`], [Disable spiOverJtag model auto-detection; use explicit bridge],
  [`--scan-usb`], [Scan USB to display connected probes],
  [`--read-dna`], [Read DNA (Xilinx FPGA only)],
  [`--read-xadc`], [Read XADC (Xilinx FPGA only)],
  [`--read-register <reg>`], [Read Status Register (Xilinx FPGA only)],
  [`--conmcu`], [Connect JTAG to MCU],
  [`--mcufw <file>`], [Microcontroller firmware file],
)

==== 4.5 Typical Workflows ====

**Program SRAM (volatile):**
`openFPGALoader -b arty design.bit`

**Program external flash (non-volatile):**
`openFPGALoader -b arty -f design.bit`

**Detect chain and flash:**
`openFPGALoader -c ft2232 --detect --detect-external-flash`

**Dump flash:**
`openFPGALoader -c ft2232 --dump-flash output.bin`

**Bulk erase:**
`openFPGALoader -c ft2232 --bulk-erase`

**Lattice MachXO3LF internal Flash dump:**
`openFPGALoader -c ft2232 --dump-flash machxo_internal.bin`

**Lattice MachXO3LF internal Flash erase (CFG sector only):**
`openFPGALoader -c ft2232 --bulk-erase --flash-sector CFG`

**XPCU with firmware skip:**
`openFPGALoader -c xilinxPlatformCableUsb --skip-probe-firmware-upload design.bit`

**Spartan-6 flash via JTAG bridge:**
`openFPGALoader -c ft2232 -f --bridge design.bit`

---

=== 5. Xilinx Platform Cable USB (XPCU) -- Fork Feature ===

This fork adds comprehensive support for the Xilinx Platform Cable USB programming adapter.

==== 5.1 Firmware Architecture ====

The XPCU uses a Cypress FX2 microcontroller that requires firmware upload. Multiple firmware variants exist for different XPCU hardware revisions:

#grid(
  columns: (2fr, 2fr, 3fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Firmware", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Target Hardware", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Notes", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [`xusb_xp2.hex`], [XPC2 (standard)], [Primary firmware],
  [`xusb_xp2_loader.hex`], [XPC2 (cold boot loader)], [Loader for cold PID 03fd:0013],
  [`xusb_xp2_loader_v3.hex`], [XPC2 variant], [Alternative loader],
  [`xusb_xlp.hex`], [Xilinx License Protector], [XLP device],
  [`xusb_xup.hex`], [XUP boards], [XUP adapter],
  [`xusb_xse.hex`], [XSE boards], [XSE adapter],
  [`xusb_emb.hex`], [Embedded/XSE], [Embedded variant],
  [`xusb_xpr.hex`], [Xilinx Platform Router], [XPR adapter],
  [`xusbdfwu.hex`], [DFU update firmware], [Fallback DFU mode],
)

==== 5.2 USB Identifiers ====

#grid(
  columns: (1.2fr, 1.5fr, 3.5fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("PID", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("State", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Description", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [`03fd:0008`], [Initialized], [Firmware loaded, ready for JTAG commands],
  [`03fd:000d`], [Cold state], [Requires firmware upload],
  [`03fd:0013`], [Cold loader], [Requires loader firmware upload first],
)

==== 5.3 Operation Modes ====

1. **Normal mode:** Cold device detected -> loader firmware uploaded -> main firmware uploaded -> device re-enumerates as 03fd:0008 -> JTAG operations begin.

2. **Skip firmware upload:** If device is already at PID 03fd:0008 (pre-initialized by another tool), use `--skip-probe-firmware-upload` to connect directly.

3. **Direct XP2 firmware:** For cold PID 03fd:0013, `--xpcu-direct-xp2-firmware` uploads `xusb_xp2.hex` directly instead of the loader sequence.

==== 5.4 Spartan-6 SPI Bridge ====

Spartan-6 devices require a bridge bitstream to enable SPI flash access over JTAG. This fork includes ISE-derived `.cor` bridge files for multiple Spartan-6 device families, with automatic lookup by FPGA model name. The bridge files are stored in `spiOverJtag/from_ise/spartan-6/`.

Supported bridge formats: `.bit`, `.bit.gz`, `.cor`

---

=== 6. Lattice MachXO2/MachXO3/MachXO3LF -- Fork Enhancements ===

This fork adds internal Flash/NVCM dump and erase support for Lattice MachXO2, MachXO3L, and MachXO3LF families.

==== 6.1 Internal Flash Architecture ====

Unlike external SPI flash, these devices have on-chip non-volatile configuration memory accessible through Lattice ISC (In-System Configuration) commands over JTAG.

- **CFG (Configuration):** Main configuration bitstream data
- **UFM (User Frame Memory):** User-programmable non-volatile frames
- **FEATURE:** Feature bits controlling configuration behavior and port enables
- **SRAM:** On-chip SRAM

==== 6.2 MachXO3LF-9400C Density Layout ====

#grid(
  columns: (1.5fr, 1.2fr, 1.2fr, 1fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Section", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Pages", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Page Size", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Total", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [CFG], [12539], [16 bytes], [~201 KB],
  [UFM], [3582], [16 bytes], [~57 KB],
  [Total internal], [16121], [16 bytes], [257936 bytes],
)

==== 6.3 Internal Dump and Erase Commands ====

**Dump internal flash:**
`openFPGALoader -c ft2232 --dump-flash machxo_internal.bin`

**Dump with offset and size:**
`openFPGALoader -c ft2232 --dump-flash -o 0x1000 --file-size 0x2000 window.bin`

**Erase specific sector:**
`openFPGALoader -c ft2232 --bulk-erase --flash-sector CFG`

**Warning:** `--flash-sector ALL` also erases FEATURE bits and SRAM. Feature bits control configuration behavior and port enables -- erasing them requires reconfiguration of the device.

---

=== 7. Limitations and Known Issues ===

This section documents the confirmed limitations of openFPGALoader as observed in the codebase. Users should be aware of these constraints before relying on specific features.

==== 7.1 JTAG Protocol Limitations ====

- **Single TAP chain only:** openFPGALoader assumes a single linear JTAG TAP chain. Devices with multiple independent TAP chains, cross-chip daisy-chains with breakpoints, or reconfigurable chain topology are not supported.

- **IR length mismatch risk:** Devices with non-standard IR lengths (e.g., Virtex-7 H-series with 22-38 bit IR, Virtex-7 2000T with 24-bit IR) require correct IR shifting. If a device's IR length is not correctly enumerated in `part.hpp`, JTAG commands may corrupt the IR shift register of downstream devices.

- **Maximum chain depth:** While there is no hardcoded limit, very long JTAG chains (10+ devices) may experience reliability issues with bitbang-based cables at high clock frequencies due to timing margins.

- **Bypass mode:** Non-FPGA devices in the chain (ARM Cortex-A9/A53 debug ports, GD32 microcontrollers) are detected and can be listed via `--misc-device`, but are not programmable through openFPGALoader.

- **No Boundary-Scan testing:** openFPGALoader does not perform IEEE 1149.1 boundary-scan verification. It can read IDCODE and shift IR/DR, but does not validate signal integrity or perform automated test patterns.

==== 7.2 Vendor-Specific Limitations ====

**Xilinx:**

- **Virtex-5 support is partial/incomplete.** Only Virtex-4 (`xc4vfx100`) and Virtex-6/7 are reliably listed. Virtex-5 devices may not be detected.

- **ZynqMP PL programming requires manual JTAG_CTRL write.** The Zynq UltraScale+ MPSoC powers up with PL TAP and ARM DAP disabled. The user must write `0x03` to `JTAG_CTRL` register, perform a reset-to-initialize (RTI), and re-scan the chain before PL programming is possible.

- **XCVR/High-speed transceiver configuration:** openFPGALoader cannot program GT/GTX transceiver settings. The bitstream must contain all transceiver configuration.

- **Dual-image / multi-boot:** Partial support for primary/secondary flash via `--target-flash` and `--secondary-bitstream` on UltraScale boards, but not all board combinations are tested.

- **Secure bitstreams (.bba, encrypted):** Not supported. Only unencrypted `.bit`, `.jed`, and `.mcs` formats are programmable.

- **Partial reconfiguration:** Not supported. openFPGALoader performs full configuration only.

- **DMA-less cables:** JTAG transfer speed depends entirely on the cable backend. Bitbang-mode cables (FT232RL, libgpiod) are significantly slower than MPSSE-mode cables.

**Altera/Intel:**

- **Configuration modes limited:** Supports Passive Serial (PS) and AS modes through JTAG. Does not support Active Serial (AS), JTAG configuration of HPS (FPGA Manager), or raw configuration modes beyond what is documented in the Altera driver.

- **MAX 10 PCON (Post-Configuration ON):** Limited support. PCON operations require specific JTAG IR codes that may not be fully implemented for all MAX 10 sub-families.

- **Arria 10 HPS programming:** HPS (ARM Cortex-A9) flash programming is not supported. Only FPGA fabric configuration.

- **Agilex support is experimental:** Agilex 3 and 5 devices are listed in the device database but have not been extensively tested. Programming reliability is not guaranteed.

- **PAC (Platform Archive Compiler) / .sof files:** `.sof` format parsing is not implemented. Only `.raw`/`.bin` and `.pof` formats are supported for Altera devices.

**Lattice:**

- **iCE40 programming is SPI-only (SSPI):** iCE40 devices are programmed through the FTDI SSPI interface, not through standard JTAG. This requires an FTDI-based cable in serial mode.

- **ECP5 encrypted bitstreams:** Not supported.

- **CrossLink-NX / Certus-NX:** Basic JTAG programming is supported, but advanced features (secure boot, fuse programming) are not implemented.

- **MachXO3LF JEDEC handling:** Diamond-generated bitstreams may contain trailing all-zero TAG DATA blocks. The fork has patched this, but users should verify bitstream integrity.

**Gowin:**

- **Limited to `.fs` format:** Gowin proprietary `.fs` (Flash Stream) format is supported, but `.bin` and `.hex` formats for Gowin devices are not parsed.

- **User flash (`--user-flash`):** Only supported on Gowin LittleBee boards.

- **GW5A series:** Newly added support, testing may be incomplete.

**Efinix:**

- **FTDI cable required:** Efinix devices require an FTDI-based cable for programming (either SPI or JTAG mode with specific Efinix board pinout).

- **Titanium series:** Basic support exists, but advanced features (HBM configuration, PCIe link training) are not handled.

**Anlogic:**

- **Proprietary cable:** Requires the Anlogic-specific cable or a compatible FTDI adapter with the correct pin mapping.

- **Limited device list:** Only Eagle D20/S20 and Elf2 EF2M45 are in the device database. Newer Anlogic families (Phoenix, etc.) are not supported.

**CologneChip:**

- **GateMate only:** Only the GateMate GM1Ax series is supported.

- **FTDI required:** Requires FTDI cable with specific board configuration.

==== 7.3 Flash Programming Limitations ====

- **SPI flash detection is JEDEC-ID based:** Flash chips not in `spiFlashdb.hpp` will not be auto-detected. The `--external-flash-type` flag can force a specific JEDEC ID or model name.

- **Quad mode is chip-specific:** Quad Enable (QE) bit location varies by manufacturer and chip. If a flash chip's QE setting is not correctly defined in the database, `--enable-quad` / `--disable-quad` may not work.

- **32 KB erase (subsector):** Not all flash chips support 4 KB subsector erase. The database tracks this, but erasing a protected flash with only 64 KB sector granularity is slower and less efficient.

- **Flash protection bits:** BP (Block Protect), TB (Top/Bottom), and Global Lock bit locations are chip-specific. Incorrect database entries can cause `--protect-flash` / `--unprotect-flash` to lock the wrong regions.

- **Dump size must be known:** For `--dump-flash` without `--file-size`, the tool uses the known capacity from the flash database. If the flash chip is not in the database, the user must specify `--file-size`.

- **BPI flash support is limited:** Only the `ypcb003381p1` board uses BPI flash. BPI support is minimal compared to SPI.

- **Flash over JTAG bridge:** For Xilinx devices with external flash, the SPI-over-JTAG bridge must be loaded into SRAM first. This requires the correct bridge bitstream file. The fork adds automatic Spartan-6 `.cor` bridge detection, but other device families may need manual `--bridge` specification.

- **Altera MAX10 flash is accessed via SVF:** Unlike other vendors where SPI commands go over a JTAG bridge, MAX10 flash programming requires SVF scripts or specific JTAG IR/DR sequences. Support is partial.

==== 7.4 Cable/Adapter Limitations ====

- **FTDI bitbang mode is slow:** Cables using bitbang mode (FT232RL, FT231X) are limited to ~1-2 MHz JTAG clock effectively. MPSSE mode (FT2232, FT4232) supports higher frequencies.

- **USB Blaster II/III firmware upload:** Requires the correct firmware file to be available. Cold devices may fail if firmware cannot be uploaded (USB permissions, driver conflicts).

- **XPCU (Xilinx Platform Cable USB) driver conflicts:** On Windows, the Cypress USB driver may conflict with the libusb backend. The fork includes Windows driver packages (`externals/xilinx-usb-driver/`) and libwdi for driver installation via Zadig.

- **XPCU firmware variants:** Different XPCU hardware revisions require different firmware files. The fork ships with 8 firmware variants from ISE 14.7. If the device has an unrecognized firmware signature, programming will fail.

- **CMSIS-DAP v1 requires hidapi:** The hidapi library adds cross-compilation complexity. CMSIS-DAP v2 (libusb-based) is preferred.

- **Remote Bitbang / XVC are Linux-only:** These communication modes are disabled on Windows builds.

- **libgpiod is Linux-only:** GPIO bitbang mode requires Linux with libgpiod support.

- **DFU mode is board-specific:** DFU programming only works for boards that implement the DFU bootloader (Fomu, iCEBreaker-bitsy, OrangeCrab, ULX3S, etc.). VID/PID must match.

- **Multiple identical cables:** When multiple cables of the same type are connected, selection requires `--cable-index`, `--busdev-num`, or `--usb-serial-num`. Auto-detection may pick the wrong device.

==== 7.5 Build and Platform Limitations ====

- **Windows native build requires MSYS2/MinGW:** Native Windows builds require the MinGW-w64 toolchain. The fork provides Docker-based cross-compilation as the primary Windows build path.

- **libftdi cross-compilation:** Building with FTDI support on Windows requires libftdi compiled for the target platform. The cross-compilation Docker setup handles this automatically.

- **Static linking:** `BUILD_STATIC=ON` links libstdc++ statically but may leave other dependencies (libusb, libftdi) dynamic. Fully static builds are not guaranteed.

- **No macOS-specific cable drivers:** While the project is reported to work on macOS and OpenBSD, some cable drivers (XPCU, libgpiod) are Linux/Windows-specific.

- **No GUI:** openFPGALoader is a CLI tool only. No graphical interface exists.

- **No incremental / partial programming:** The tool performs full configuration. Partial reconfiguration, FPGA manager streaming, and incremental update modes are not supported.

- **No automated testing framework:** There is no unit test or integration test suite visible in the repository. Code quality relies on manual testing and community feedback.

- **Single-threaded operation:** The programming pipeline is sequential. Concurrent programming of multiple devices or boards is not supported.

- **No web server / remote API:** Beyond the XVC server and Remote Bitbang client, there is no HTTP-based API for integration with CI/CD or fleet management systems.

- **No signed firmware verification:** The tool does not verify the cryptographic signature of bitstream files. Loading a malicious or corrupted bitstream is possible.

---

=== 8. Build Instructions ===

==== 8.1 Linux Native Build ====

```
cmake -B build
cmake --build build -j$(nproc)
sudo cmake --install build
```

==== 8.2 Windows Cross-Compile (Docker) ====

```
docker compose -f docker-compose.cross-windows.yml up
```

Produces Windows binaries in `dist/docker-windows/`.

==== 8.3 Linux Docker Deploy ====

```
docker compose -f docker-compose.deploy-linux.yml up
```

Produces Alpine Linux packages in `dist/docker-linux/`.

==== 8.4 Windows Native (MSYS2) ====

```
pacman -S mingw-w64-x86_64-cmake mingw-w64-x86_64-toolchain \
  mingw-w64-x86_64-libusb mingw-w64-x86_64-libftdi1 \
  mingw-w64-x86_64-hidapi mingw-w64-x86_64-zlib

cmake -B build -G "MinGW Makefiles"
cmake --build build
```

---

=== 9. Environment Variables ====

- **`OPENFPGALOADER_SOJ_DIR`**: Path to spiOverJtag bridge bitstreams (default: install share dir)

---

=== 10. File Format Reference ====

**10.1 Xilinx Bitstream (.bit)**
Binary format containing configuration frames for Xilinx FPGAs. Supports gzip compression (`.bit.gz`). Parsed by `ConfigBitstreamParser` which extracts frame addresses and data, handling both raw and compressed variants.

**10.2 JEDec (.jed)**
ASCII format for array programming (Xilinx CPLDs, Lattice devices). Contains array coordinates and logic states. Parsed by `JedParser`.

**10.3 MCS (.mcs)**
Binary format used by both Xilinx and Lattice for flash programming. Contains raw flash image data with addressing information. Parsed by `McsParser`.

**10.4 POF (.pof)**
Altera/Intel flash programming format. Contains configuration data for serial configuration devices. Parsed by `PofParser`.

**10.5 Raw Binary (.raw/.bin)**
Unstructured binary data written directly to flash or SRAM. Parsed by `RawParser`.

**10.6 Flash Stream (.fs)**
Gowin proprietary format for flash programming. Supports gzip compression (`.fs.gz`). Parsed by `FsParser`.

**10.7 Lattice Bitstream (.bit)**
Lattice-specific binary bitstream format. Parsed by `LatticeBitParser` with support for MachXO and ECP5 families.

**10.8 Intel HEX (.hex)**
Used for firmware files (XPCU firmware, Efinix configuration). Parsed by `IhexParser` and `EfinixHexParser`.

**10.9 COR (.cor)**
Xilinx ISE-generated configuration files for SPI bridge bitstreams on Spartan-6 devices. Fork feature for automatic bridge detection.

**10.10 FEABits (.fea)**
Lattice feature file format. Parsed by `FeaParser`.

---

=== Appendix A: Source File Index ===

#grid(
  columns: (2.8fr, 3.5fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Source File", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Purpose", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [`main.cpp`], [CLI entry, argument parsing, orchestration],
  [`jtag.cpp` / `jtag.hpp`], [JTAG TAP controller, chain scanning],
  [`device.cpp` / `device.hpp`], [Abstract device base class],
  [`cable.hpp`], [Cable definitions, VID/PID mappings],
  [`board.hpp`], [Board configurations, pin assignments],
  [`part.hpp`], [FPGA IDCODE database, manufacturer IDs],
  [`spiFlash.cpp` / `spiFlash.hpp`], [SPI flash operations, section management],
  [`spiFlashdb.hpp`], [SPI flash chip database (JEDEC IDs, sectors, protection)],
  [`flashInterface.hpp`], [Abstract SPI interface mixin],
  [`bpiFlash.cpp` / `bpiFlash.hpp`], [BPI flash support],
  [`configBitstreamParser.cpp`], [Bitstream parser base class],
  [`jedParser.cpp`], [JEDec format parser],
  [`mcsParser.cpp`], [MCS format parser],
  [`rawParser.cpp`], [Raw binary parser],
  [`xilinx.cpp` / `xilinx.hpp`], [Xilinx programming, register reads],
  [`altera.cpp` / `altera.hpp`], [Altera/Intel programming, SVF, MAX10],
  [`lattice.cpp` / `lattice.hpp`], [Lattice programming (MachXO, ECP5, ISC)],
  [`gowin.cpp` / `gowin.hpp`], [Gowin programming, user flash],
  [`anlogic.cpp` / `anlogic.hpp`], [Anlogic programming],
  [`efinix.cpp` / `efinix.hpp`], [Efinix programming (Trion, Titanium)],
  [`colognechip.cpp` / `colognechip.hpp`], [CologneChip GateMate programming],
  [`ice40.cpp` / `ice40.hpp`], [iCE40 programming via SSPI],
  [`latticeSSPI.cpp` / `latticeSSPI.hpp`], [Lattice SSPI interface],
  [`latticeBitParser.cpp`], [Lattice bitstream parser],
  [`feaparser.cpp`], [Lattice FEABits parser],
  [`fsparser.cpp`], [Gowin .fs format parser],
  [`anlogicBitParser.cpp`], [Anlogic bitstream parser],
  [`efinixHexParser.cpp`], [Efinix Intel HEX parser],
  [`colognechipCfgParser.cpp`], [CologneChip .cfg parser],
  [`pofParser.cpp`], [Altera .pof parser],
  [`ihexParser.cpp`], [Intel HEX parser (XPCU firmware)],
  [`dfuFileParser.cpp`], [DFU file parser],
  [`ftdiJtagMPSSE.cpp`], [FTDI MPSSE engine (ft2232, ft4232, etc.)],
  [`ftdiJtagBitbang.cpp`], [FTDI bitbang engine (ft232RL, ft231X)],
  [`ftdipp_mpsse.cpp`], [libftdi wrapper (MPSSE mode)],
  [`ftdispi.cpp`], [FTDI SPI interface],
  [`ch552_jtag.cpp`], [CH552 JTAG adapter (TangNano)],
  [`ch347jtag.cpp`], [CH347 JTAG adapter],
  [`gwu2x_jtag.cpp`], [Gowin GWU2X programmer],
  [`esp_usb_jtag.cpp`], [ESP32-S3/C3 USB JTAG],
  [`anlogicCable.cpp`], [Anlogic proprietary cable],
  [`jlink.cpp`], [Segger J-Link adapter],
  [`dirtyJtag.cpp`], [DirtyJTAG (STM32F1-based)],
  [`usbBlaster.cpp`], [Altera USB Blaster I],
  [`fx2_ll.cpp`], [Cypress FX2 low-level (Blaster II/III, XPCU)],
  [`cmsisDAP.cpp`], [CMSIS-DAP v1 (HIDAPI) + v2 (libusb)],
  [`dfu.cpp`], [DFU mode for ECP5/iCE40 boards],
  [`svf_jtag.cpp`], [SVF JTAG script execution],
  [`xvc_client.cpp`], [Xilinx Virtual Cable TCP client],
  [`xvc_server.cpp`], [Xilinx Virtual Cable TCP server],
  [`xilinxPlatformCableUSB.cpp`], [XPCU driver (fork feature)],
  [`libgpiodJtagBitbang.cpp`], [Linux libgpiod GPIO bitbang],
  [`jetsonNanoJtagBitbang.cpp`], [NVIDIA Jetson GPIO bitbang],
  [`remoteBitbang_client.cpp`], [Remote Bitbang TCP client],
  [`libusb_ll.cpp`], [libusb low-level helpers],
  [`pathHelper.cpp` / `pathHelper.hpp`], [Cross-platform path resolution],
  [`display.cpp` / `display.hpp`], [Terminal output, colors, progress],
  [`progressBar.cpp` / `progressBar.hpp`], [Animated progress bar],
  [`common.cpp` / `common.hpp`], [Shared utilities],
  [`board.hpp`], [Board database (80+ boards)],
  [`cxxopts.hpp`], [CLI argument parsing (bundled)],
)

---

=== Appendix B: Cable Reference ===

Complete list of built-in cable configurations from `src/cable.hpp`:

#grid(
  columns: (1.8fr, 1.3fr, 0.8fr, 3fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("Cable", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("VID:PID", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Chip", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Description", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [digilent], [0403:6010], [FT2232], [Digilent Adept (Arty, Basys3, etc.)],
  [digilent_b], [0403:6010], [FT2232], [Digilent interface B (Genesys2, NexysVideo)],
  [digilent_hs2], [0403:6014], [FT4232], [Digilent HS2 (ZedBoard, etc.)],
  [digilent_ft4232], [0403:6011], [FT4232], [Digilent FT4232 (SP701)],
  [ft2232], [0403:6010], [FT2232], [Generic FT2232H],
  [ft2232_b], [0403:6010], [FT2232], [FT2232H interface B],
  [ft232], [0403:6001], [FT232R], [Single-channel FT232R MPSSE],
  [ft232RL], [0403:6001], [FT232R], [FT232R bitbang mode],
  [ft231X], [0403:6015], [FT231X], [FT231X bitbang (ULX3S, etc.)],
  [ft4232], [0403:6011], [FT4232], [Generic FT4232H interface A],
  [ft4232_b], [0403:6011], [FT4232], [FT4232H interface B],
  [ft4232hp_b], [0403:6011], [FT4232], [FT4232H port B (GR740)],
  [papilio], [0403:bcdf], [FT2232], [Gadgetfactory Papilio],
  [numato], [0403:6010], [FT2232], [Numato Mimas/Neos],
  [numato-neso], [0403:6010], [FT2232], [Numato Neso A7],
  [jlink], [1366:0101], [J-Link], [Segger J-Link (OB, EDU, PRO)],
  [dirtyJtag], [1209:0d31], [STM32], [DirtyJTAG on STM32F1],
  [usb-blaster], [09fb:6001], [FX2], [Altera USB Blaster I],
  [usb-blasterII], [09fb:6002], [FX2], [Altera USB Blaster II (firmware upload)],
  [usb-blasterIII], [09fb:6003], [FX2], [Altera USB Blaster III],
  [cmsisdap], [auto], [HID/USB], [CMSIS-DAP auto-detect (v1 HIDAPI + v2 libusb)],
  [xilinxPlatformCableUsb], [03fd:0008/0d/13], [FX2], [Xilinx Platform Cable USB (fork)],
  [tangnano], [303a:1001], [CH552], [TangNano CH552 JTAG],
  [ch347], [1a86:8079], [CH347], [WCH CH347 JTAG adapter],
  [gwu2x], [2a19:002x], [GWU2X], [Gowin GWU2X programmer],
  [esp32s3], [303a:1001], [ESP32], [ESP32-S3 USB JTAG],
  [anlogicCable], [0483:a041], [STM32], [Anlogic proprietary cable],
  [ecpix5-debug], [0483:a041], [STM32], [LambdaConcept ECPIX-5 debug],
  [jtag-smt2-nc], [03fd:0008], [FX2], [SMT2 non-configurable JTAG],
  [gatemate_pgm], [0403:6010], [FT2232], [CologneChip GateMate programmer],
  [gatemate_evb_jtag], [0403:6010], [FT2232], [GateMate EVB JTAG mode],
  [efinix_jtag_ft4232], [0403:6011], [FT4232], [Efinix JTAG via FT4232],
  [xvc-client], [auto], [TCP], [XVC TCP client],
  [xvc-server], [auto], [TCP], [XVC TCP server],
  [libgpiod_bitbang], [auto], [GPIO], [Linux libgpiod GPIO bitbang],
  [jetsonNano_bitbang], [auto], [GPIO], [NVIDIA Jetson GPIO bitbang],
  [remoteBitbang], [auto], [TCP], [Remote Bitbang TCP client],
  [dfu], [auto], [DFU], [DFU mode (Fomu, iCEBreaker-bitsy, etc.)],
)

---

=== Appendix C: JTAG IDCODE Format ===

JTAG IDCODEs are 32-bit values with the following structure:

- **Bits [0-3]:** Version (usually 0)
- **Bits [4-11]:** Manufacturer ID (IEEE OUI)
- **Bits [12-27]:** Part number
- **Bits [28-31]:** Reserved (1111b)

==== Manufacturer IDs ====

#grid(
  columns: (1fr, 2fr, 3fr),
  columns-gap: 0.3em,
  row-gap: 0.15em,
  gutter: none,
  [#lozenge("ID", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Manufacturer", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [#lozenge("Notes", fill: rgb("1a3a5c"), stroke: none, radius: 3pt, color: white)],
  [0x000], [CologneChip / Efinix T4/T8], [Shared ID],
  [0x021], [Lattice], [Standard Lattice devices],
  [0x049], [Xilinx], [Legacy Xilinx devices],
  [0x06e], [Altera], [Standard Altera/Intel devices],
  [0x093], [Xilinx], [Modern Xilinx (Spartan6, 7-series, US+, Zynq)],
  [0x40d], [Gowin], [All Gowin devices],
  [0x53c], [Efinix], [Efinix Trion/Titanium],
  [0x61a], [Anlogic], [Anlogic primary ID],
  [0x61b], [Anlogic], [Anlogic secondary ID],
  [0x093 (ZynqMP cfgn)], [Xilinx (ZynqMP cfgn)], [ZynqMP before JTAG_CTRL write],
)

---

=== Appendix D: Fork Changes Summary ===

This fork diverges from the upstream `trabucayre/openFPGALoader` at merge base `d52abf70` with 23+ commits of changes.

==== Major Additions ====

1. Windows cross-compilation and packaging (Docker + MSYS2)
2. Xilinx Platform Cable USB (XPCU) full driver with firmware management
3. ISE-derived XPCU firmware files (8 variants)
4. Spartan-6 SPI bridge `.cor` files with automatic detection
5. External SPI flash detection (`--detect-external-flash`)
6. Forced flash type selection (`--external-flash-type`)
7. SPI flash listing (`--list-flash`)
8. Auto dump-size from flash database
9. Improved dump failure handling (zero-size rejection, short write detection)
10. Lattice MachXO2/MachXO3/3LF internal Flash/NVCM dump
11. Lattice MachXO2/MachXO3/3LF internal Flash erase (sector-selective)
12. MachXO3LF-9400C support with JEDEC handling
13. Macronix MX77L25650F flash support (including RDID alias 0x77b80a)
14. Probe selection by bus/device number
15. USB scan improvements
16. XPCU endpoint discovery and environment override
17. Retry and recovery handling for XPCU transfers
18. Control-bitbang fallback for XPCU
19. Windows driver packages (libwdi, xilinx-usb-driver)
20. CMakePresets.json for Windows debug builds
21. Linux Docker deployment pipeline (Alpine)
22. Bridge handling for `.bit`, `.bit.gz`, `.cor` inputs
23. Arria 10 / Virtex-4 device support additions

==== Key Files Added/Modified by Fork ====

- `src/xilinxPlatformCableUSB.cpp` (new -- XPCU driver)
- `src/pathHelper.cpp` (new -- cross-platform path resolution)
- `ise_programmer_bins/` (new -- XPCU firmware files)
- `spiOverJtag/from_ise/spartan-6/` (new -- SPI bridge `.cor` files)
- `externals/libwdi/` (new submodule)
- `externals/xilinx-usb-driver/` (new submodule)
- `docker/cross/windows/` (new -- cross-compile Dockerfiles)
- `docker/deploy/linux/` (new -- Linux deploy Dockerfiles)
- `docker-compose.cross-windows.yml` (new)
- `docker-compose.deploy-linux.yml` (new)
- `CMakePresets.json` (new)
- `CMakeLists.txt` (modified -- Windows cross-compile, install rules)
- `src/spiFlashdb.hpp` (modified -- added MX77L25650F)
- `src/part.hpp` (modified -- added Arria 10, Virtex-4 entries)
- `src/lattice.cpp` (modified -- internal Flash/NVCM dump/erase)
- `src/main.cpp` (modified -- fork-specific CLI options)
- `src/xilinx.cpp` (modified -- bridge auto-detection)

---

=== Appendix E: Related Tools and Alternatives ====

- **Xilinx Vivado/Impact** -- Proprietary Xilinx toolchain (required for some features)
- **Intel Quartus** -- Proprietary Altera/Intel toolchain
- **Lattice Diamond** -- Proprietary Lattice toolchain
- **Gowin IDE** -- Proprietary Gowin toolchain
- **yosys + nextpnr + icepack** -- Open-source toolchain for iCE40/ECP5
- **SymbiFlow** -- Open-source Xilinx toolchain
- **urJTAG** -- Another open-source JTAG tool (boundary-scan focused)
- **OpenOCD** -- Open On-Chip Debugger (ARM-focused but supports some FPGAs)
- **flashrom** -- Standalone SPI flash programming tool

---

=== Document Metadata ===

- **Generated:** 2025
- **Based on:** openFPGALoader v1.1.2 (xsession fork)
- **Source:** `C:\Users\livanyi\Desktop\WORK\GIT\openFPGALoader`
- **License:** Apache 2.0