# Release Notes - xsession openFPGALoader Fork

These notes summarize the changes in this fork compared with upstream
`trabucayre/openFPGALoader`.

Comparison baseline:

- Upstream remote checked: `https://github.com/trabucayre/openFPGALoader.git`
- Fork merge base: `d52abf7059d4471957775c3ed067437715174dd5`
- Current fork head inspected: `9092deec Working with Lattice`
- Upstream master inspected: `c8452092f9a31b1bfe8f2e09beadf2d0cc51e3f2`
- Fork commits on top of the merge base: 23, plus current local working-tree changes

## Highlights

- Added Windows-focused build, packaging, and deployment support.
- Added and improved Xilinx Platform Cable USB / XPCU support, including firmware handling and debug controls.
- Added ISE-derived Xilinx FX2 firmware files and Spartan-6 SPI bridge `.cor` files.
- Improved Spartan-6 external SPI flash workflows through Xilinx JTAG bridges.
- Added external SPI flash discovery, forced flash type selection, flash listing, and safer dump behavior.
- Added Lattice MachXO2/MachXO3 internal Flash/NVCM dump and erase support.
- Added support and troubleshooting for `LCMXO3LF-9400C`.
- Added support for Macronix `MX77L25650F`, including observed RDID alias `0x77b80a`.

## Windows Build And Packaging

- Added Docker-based cross-Windows build support.
- Added `docker-compose.cross-windows.yml`.
- Added `docker/cross/windows/alpine.Dockerfile`.
- Added `docker/cross/windows/toolchain-mingw64.cmake`.
- Added `scripts/docker-cross-windows.sh`.
- Added `DOCKER_CROSS_WINDOWS.md` and `DEPLOYMENT.md`.
- Added GitHub workflow support for binary builds in `.github/workflows/build-binaries.yml`.
- Added `CMakePresets.json` with Windows debug presets.
- Updated CMake install logic to package additional bridge and firmware assets.

## Xilinx Platform Cable USB / XPCU

- Added and expanded the `xilinxPlatformCableUSB` backend.
- Added alternate cable identities, including cold-loader and initialized XPCU paths.
- Added firmware selection for Xilinx Platform Cable USB variants.
- Added CLI options:

```text
--probe-firmware
--skip-probe-firmware-upload
--xpcu-direct-xp2-firmware
```

- Added packaged Xilinx USB firmware files:

```text
ise_programmer_bins/xusb_emb.hex
ise_programmer_bins/xusb_xlp.hex
ise_programmer_bins/xusb_xp2.hex
ise_programmer_bins/xusb_xp2_loader.hex
ise_programmer_bins/xusb_xpr.hex
ise_programmer_bins/xusb_xse.hex
ise_programmer_bins/xusb_xup.hex
ise_programmer_bins/xusbdfwu.hex
```

- Added `scripts/extract-xusb-loader-from-sys.ps1` for extracting loader firmware from the Xilinx driver.
- Added firmware lookup through packaged share data and environment/configurable paths.
- Added retry and recovery handling for XPCU USB control and bulk transfers.
- Added XPCU endpoint discovery and environment override support.
- Added control-bitbang fallback and accelerated-transfer setup paths.
- Improved XPCU debug output, status reporting, and transfer diagnostics.
- Added technical XPCU documentation:

```text
XILINX_PLATFORM_CABLE_USB_WINDOWS.md
docs/xilinx-platform-cable-usb-technical-reproduction.md
docs/xilinx-platform-cable-usb-windows-debug.md
docs/diagrams/xpcu-firmware-state-machine.mmd
docs/diagrams/xpcu-firmware-state-machine.svg
```

## Xilinx / Spartan-6 External SPI Flash

- Added `.cor` bridge-file support.
- Added ISE-derived Spartan-6 SPI bridge `.cor` files under:

```text
spiOverJtag/from_ise/spartan-6/
```

- Added automatic Spartan-6 `.cor` bridge lookup by FPGA model.
- Improved Xilinx external flash code paths for Spartan-6 targets.
- Added bridge handling for `.bit`, `.bit.gz`, and `.cor` inputs.
- Improved external flash write and dump handling through Xilinx JTAG-to-SPI bridges.
- Added troubleshooting and investigation notes:

```text
docs/digilent-hs3-vs-xpcu-dump-flash-codepath.md
docs/spartan6-hs3-m25p40-write-stall.md
docs/spartan6-xpcu-spi-debug-log.md
```

## SPI Flash Features

- Added `--detect-external-flash` to detect/display external SPI flash chip information.
- Added `--external-flash-type` to force an SPI flash by JEDEC ID or model name.
- Added `--list-flash` to list supported SPI flash chips.
- Added automatic dump size calculation for known SPI flash chips.
- When `--file-size` is omitted or set to `0`, SPI flash dump now reads from the selected offset to the end of the detected flash.
- Improved dump failure handling:

```text
- invalid zero-size dumps are rejected
- short writes are detected
- close errors are detected
- failed partial dump files are removed
- dump failures propagate to the process exit status
```

- Added support for Macronix `MX77L25650F`:

```text
canonical JEDEC ID: 0xc27519
observed alias:    0x77b80a
capacity:          256 Mbit / 32 MiB
layout:            512 x 64 KiB sectors
```

## Lattice MachXO2 / MachXO3 / MachXO3LF

- Documented the difference between Lattice internal Flash/NVCM and external SPI flash programming paths.
- Added `LCMXO3LF-9400C` support details and troubleshooting.
- Improved MachXO3LF-9400C JEDEC handling for Diamond-generated files with trailing all-zero `TAG DATA`.
- Added density layout handling for the 9400 device:

```text
CFG pages: 12539
UFM pages: 3582
page size: 16 bytes
total internal dump size: 257936 bytes
```

- Added internal Flash/NVCM dump support for MachXO2, MachXO3L, and MachXO3LF.
- For these devices, `--dump-flash` now uses Lattice ISC/JTAG readback instead of the external SPI flash path.
- Internal dumps contain CFG data followed by UFM data.

Examples:

```bash
openFPGALoader -c ft2232 --dump-flash machxo_internal.bin
openFPGALoader -c ft2232 --dump-flash -o 0x1000 --file-size 0x2000 machxo_window.bin
```

- Added internal erase support for MachXO2, MachXO3L, and MachXO3LF.
- For these devices, `--bulk-erase` now uses the Lattice ISC erase flow.
- Added internal sector selection using `--flash-sector`.

Supported internal erase sectors:

```text
CFG
UFM
FEATURE
SRAM
ALL
```

Examples:

```bash
openFPGALoader -c ft2232 --bulk-erase
openFPGALoader -c ft2232 --bulk-erase --flash-sector CFG
openFPGALoader -c ft2232 --bulk-erase --flash-sector UFM
```

Warning: `ALL` also erases feature bits and SRAM. Feature bits control configuration behavior and port enables.

## Intel / Altera Device Support

- Started adding Arria 10-related support.
- Added additional ARM/JTAG chain handling notes and IDs.
- Updated FPGA support metadata in `doc/FPGAs.yml` and `src/part.hpp`.

## USB, Probe Selection, And Diagnostics

- Added or improved USB scan support.
- Added probe selection by USB bus/device number.
- Added improved cable/debug/error helper messages.
- Improved lower-level JTAG and FX2 diagnostics.
- Added helper behavior around firmware upload, re-enumeration, and initialized cable states.

## Documentation

- Added Windows Xilinx Platform Cable USB setup notes.
- Added deployment and Docker cross-build guides.
- Added external flash and XPCU debug writeups.
- Added Lattice programming, dumping, erasing, and troubleshooting sections.
- Added `DEVICE_SUPPORT_CHAT.md`.
- Added `doc/ug380.pdf` as a local Xilinx reference document.

## Submodules And Bundled Assets

- Added `externals/libwdi`.
- Added `externals/xilinx-usb-driver`.
- Added `.gitmodules`.
- Added local ISE programmer firmware files.
- Added Spartan-6 ISE SPI bridge `.cor` files.

## Commit Summary

Fork commits inspected:

```text
11121421 Add docker based deploy system
71861c85 Before changes, fail to find .bin.gz file
656923b3 Works the flash to the external flash
8ce446b4 Start working the xilinxPlatformCableUsb_alt
12587878 try to fix things to achieve to communicate with spartan-6
30dd3066 Start to add aria 10 and virtex-4 support
316e308a Add zadig as submodule
1821cac7 Update libwdi and add xilinx-usb-driver as submodule
5ec5c920 Seems like fixed the xilinxPlatformCableUsb driver/handling
8b415083 Update docs
f9c7fef9 Add programmer interfaces hexes direct from ise 14.7
3720ce23 Fix platform cable usb 2 with the right hex files
193f0597 Add more detailed documentation
9da1797f now see virtex-4
55e4c145 Update submodule
12816a00 Add more debug/error helper message
7150b898 Now with bitbang 0 works with artix-7 the xilinxPlatformCableUsb
07b1cd45 Add external flash detection and list out functionalities
b6be6104 Add .cor support
1dbfcbec ehh
735992e1 Add docs
ed38f8ed Improve XPCU reliability and Spartan-6 external flash support
9092deec Working with Lattice
```

## Scope Note

This file summarizes both committed fork changes and the current local working-tree changes at the time it was generated. Re-run the upstream comparison before publishing a final release if more commits are added.
