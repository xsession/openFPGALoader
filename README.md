# openFPGALoader

<p align="right">
  <a title="Documentation" href="https://xsession.github.io/openFPGALoader"><img src="https://img.shields.io/website.svg?label=xsession.github.io%2FopenFPGALoader&longCache=true&style=flat-square&url=https%3A%2F%2Fxsession.github.io%2FopenFPGALoader%2F&logo=GitHub"></a><!--
  -->
  <a title="'Test' workflow Status" href="https://github.com/xsession/openFPGALoader/actions/workflows/Test.yml"><img alt="'Test' workflow Status" src="https://img.shields.io/github/actions/workflow/status/xsession/openFPGALoader/Test.yml?branch=master&longCache=true&style=flat-square&label=Test&logo=github%20actions&logoColor=fff"></a><!--
  -->
  <a title="Releases" href="https://github.com/xsession/openFPGALoader/releases"><img src="https://img.shields.io/github/commits-since/xsession/openFPGALoader/latest.svg?longCache=true&style=flat-square&logo=git&logoColor=fff"></a>
</p>

<p align="center">
  <strong><a href="https://xsession.github.io/openFPGALoader/guide/first-steps/">First steps</a> • <a href="https://xsession.github.io/openFPGALoader/guide/install/">Install</a> • <a href="https://xsession.github.io/openFPGALoader/guide/troubleshooting/">Troubleshooting</a></strong> • <a href="https://xsession.github.io/openFPGALoader/guide/advanced/">Advanced usage</a>
</p>

<p align="center">
  <a href="https://xsession.github.io/openFPGALoader/architecture/">Architecture</a> • <a href="https://xsession.github.io/openFPGALoader/architecture/c4/context/">C4 model</a> • <a href="https://xsession.github.io/openFPGALoader/contributing/architecture-rules/">Contributor architecture rules</a>
</p>

Universal utility for programming FPGAs. Compatible with many boards, cables and FPGA from major manufacturers (Xilinx, Altera/Intel, Lattice, Gowin, Efinix, Anlogic, Cologne Chip). openFPGALoader works on Linux, Windows, macOS and OpenBSD.

Not sure if your hardware is supported? Check the hardware compatibility lists:

 * [FPGA compatibility list](https://xsession.github.io/openFPGALoader/compatibility/fpga/)
 * [Board compatibility list](https://xsession.github.io/openFPGALoader/compatibility/board/)
 * [Cable compatibility list](https://xsession.github.io/openFPGALoader/compatibility/cable/)

Also checkout the vendor-specific documentation:
[Anlogic](https://xsession.github.io/openFPGALoader/vendors/anlogic/),
[Cologne Chip](https://xsession.github.io/openFPGALoader/vendors/colognechip/),
[Efinix](https://xsession.github.io/openFPGALoader/vendors/efinix/),
[Gowin](https://xsession.github.io/openFPGALoader/vendors/gowin/),
[Intel/Altera](https://xsession.github.io/openFPGALoader/vendors/intel/),
[Lattice](https://xsession.github.io/openFPGALoader/vendors/lattice/),
[Xilinx](https://xsession.github.io/openFPGALoader/vendors/xilinx/).

OpenFPGALoader has a dedicated channel: [#openFPGALoader at libera.chat](https://web.libera.chat/#openFPGALoader).

## Quick Usage

`arty` in the example below is one of the many FPGA board configurations listed [here](https://xsession.github.io/openFPGALoader/compatibility/board/).

```bash
openFPGALoader -b arty arty_bitstream.bit # Loading in SRAM
openFPGALoader -b arty -f arty_bitstream.bit # Writing in flash
```

You can also specify a JTAG cable model (complete list [here](https://xsession.github.io/openFPGALoader/compatibility/cable/)) instead of the board model:

```bash
openFPGALoader -c cmsisdap fpga_bitstream.bit
```

## Usage

```
Usage: ./openFPGALoader [OPTION...] BIT_FILE
openFPGALoader -- a program to flash FPGA

      --altsetting arg          DFU interface altsetting (only for DFU mode)
      --bitstream arg           bitstream
      --secondary-bitstream arg
                                secondary bitstream (some Xilinx UltraScale
                                boards)
  -b, --board arg               board name, may be used instead of cable
  -B, --bridge arg              disable spiOverJtag model detection by
                                providing bitstream(intel/xilinx)
  -c, --cable arg               jtag interface
      --status-pin arg          JTAG mode / FTDI: GPIO pin number to use as a
                                status indicator (active low)
      --invert-read-edge        JTAG mode / FTDI: read on negative edge
                                instead of positive
      --vid arg                 probe Vendor ID
      --pid arg                 probe Product ID
      --cable-index arg         probe index (FTDI and cmsisDAP)
      --busdev-num arg          select a probe by it bus and device number
                                (bus_num:device_addr)
      --usb-serial-num arg      USB iSerial (FTDI chip serial number or ESP32
                                iSerialNumber substring)
      --ftdi-serial arg         FTDI chip serial number (Deprecated)
      --ftdi-channel arg        FTDI chip channel number (channels 0-3 map to
                                A-D)
  -d, --device arg              device to use (/dev/ttyUSBx)
      --detect                  detect FPGA, add -f to show connected flash
      --dfu                     DFU mode
      --dump-flash              Dump flash mode
      --bulk-erase              Bulk erase flash
      --enable-quad             Enable quad mode for SPI Flash
      --disable-quad            Disable quad mode for SPI Flash
      --target-flash arg        for boards with multiple flash chips (some
                                Xilinx UltraScale boards), select the target
                                flash: primary (default), secondary or both
      --external-flash          select ext flash for device with internal and
                                external storage
      --external-flash-type arg force external SPI flash type by JEDEC ID
                                (for example 0x202013) or model name
                                (for example M25P40)
      --file-size arg           provides size in Byte to dump; with
                                dump-flash, omitted or 0 means dump from
                                offset to end of known SPI flash
      --file-type arg           provides file type instead of let's deduced
                                by using extension
      --flash-sector arg        flash sector (Lattice and Altera MAX10 parts
                                only)
      --fpga-part arg           fpga model flavor + package
      --freq arg                jtag frequency (Hz)
  -f, --write-flash             write bitstream in flash (default: false)
      --index-chain arg         device index in JTAG-chain
      --misc-device arg         add JTAG non-FPGA devices <idcode,irlen,name>
      --ip arg                  IP address (XVC and remote bitbang client)
      --list-boards             list all supported boards
      --list-cables             list all supported cables
      --list-fpga               list all supported FPGA
  -m, --write-sram              write bitstream in SRAM (default: true)
  -o, --offset arg              Start address (in bytes) for read/write into
                                non volatile memory (default: 0)
      --pins arg                pin config TDI:TDO:TCK:TMS or
                                MOSI:MISO:SCK:CS[:HOLDN:WPN]
      --probe-firmware arg      firmware for JTAG probe
                                (usbBlasterII/xilinxPlatformCableUsb)
      --skip-probe-firmware-upload
                                JTAG mode / xilinxPlatformCableUsb: do not
                                upload FX2 firmware; open initialized
                                03fd:0008 directly
      --xpcu-direct-xp2-firmware
                                JTAG mode / xilinxPlatformCableUsb: for cold
                                PID 03fd:0013, upload xusb_xp2.hex directly
                                instead of xusb_xp2_loader.hex
      --protect-flash arg       protect SPI flash area
      --quiet                   Produce quiet output (no progress bar)
  -r, --reset                   reset FPGA after operations
      --scan-usb                scan USB to display connected probes
      --skip-load-bridge        skip writing bridge to SRAM when in
                                write-flash mode
      --skip-reset              skip resetting the device when in write-flash
                                mode
      --spi                     SPI mode (only for FTDI in serial mode)
      --unprotect-flash         Unprotect flash blocks
  -v, --verbose                 Produce verbose output
      --verbose-level arg       verbose level -1: quiet, 0: normal,
                                1:verbose, 2:debug
      --force-terminal-mode     force progress bar output as if connected to
                                a terminal
  -h, --help                    Give this help list
      --verify                  Verify write operation (SPI Flash only)
      --xvc                     Xilinx Virtual Cable Functions
      --port arg                Xilinx Virtual Cable and remote bitbang Port
                                (default 3721)
      --mcufw arg               Microcontroller firmware
      --conmcu                  Connect JTAG to MCU
  -D, --read-dna                Read DNA (Xilinx FPGA only)
  -X, --read-xadc               Read XADC (Xilinx FPGA only)
      --read-register arg       Read Status Register(Xilinx FPGA only)
      --user-flash arg          User flash file (Gowin LittleBee FPGA only)
  -V, --version                 Print program version
      --Version                 Print program version (Deprecated)

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.

Report bugs to <gwenhael.goavec-merou@trabucayre.com>.
```

By default **spiOverJtag** are search into `${CMAKE_INSTALL_FULL_DATAROOTDIR}`
(*/usr/local/share/* by default). It's possible to change this behaviour by
using an environment variable:

```bash
export OPENFPGALOADER_SOJ_DIR=/somewhere
openFPGALoader xxxx
```

or

```
OPENFPGALOADER_SOJ_DIR=/somewhere openFPGALoader xxxx
```

`OPENFPGALOADER_SOJ_DIR` must point to directory containing **spiOverJtag**
bitstreams.

## Sponsors/Partners

![Sponsors](https://github.com/user-attachments/assets/cb4efce1-ed0c-461c-bd05-9caeb440870d)
