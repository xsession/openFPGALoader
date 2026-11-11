# CLI reference

Complete reference for the `openFPGALoader` command line, grouped by
purpose. The typical invocation is:

```bash
openFPGALoader [options] BIT_FILE
```

Options marked *(default: X)* change the default behavior. For workflow
examples see the [usage tutorial](usage.md); for what happens underneath,
see [How it works](../how-it-works/overview.md).

## Core operations

| Option | Description |
|---|---|
| `BIT_FILE`, `--bitstream` | configuration image to program or dump (also the output name for `--dump-flash`) |
| `--secondary-bitstream` | second image for boards with two flash chips (some Xilinx UltraScale) |
| `-m, --write-sram` *(default: true)* | write the bitstream to volatile SRAM |
| `-f, --write-flash` *(default: false)* | write the bitstream to non-volatile flash |
| `-r, --reset` | reset the FPGA after the operation |
| `-o, --offset` *(default: 0)* | start address (bytes) for read/write in non-volatile memory |
| `--file-size` | number of bytes to dump; omitted or 0 means "offset to end of flash" (known chips) |
| `--file-type` | file type instead of extension-based detection (required for STDIN input) |
| `--verify` | verify the write (SPI flash) |
| `--user-flash` | user-flash image (Gowin LittleBee) |
| `--mcufw` | microcontroller firmware (Xilinx SoC MCU path) |
| `--conmcu` | connect JTAG to the MCU instead of the FPGA fabric |

## Target selection

| Option | Description |
|---|---|
| `-b, --board` | board name — implies cable, FPGA part, pins, mode (list with `--list-boards`) |
| `-c, --cable` | JTAG/SPI/DFU cable keyword (list with `--list-cables`) |
| `--fpga-part` | FPGA model+package (e.g. `xc7a35tcsg324`); needed when the board is not in the catalog |
| `--index-chain` | zero-based position of the target device in the JTAG chain |
| `--misc-device` | add a non-FPGA JTAG device `<idcode,irlen,name>` to the chain model |
| `--external-flash` | select the external flash on devices with internal + external storage |
| `--external-flash-type` | force the external SPI flash model by JEDEC ID (e.g. `0x202013`) or part name (e.g. `M25P40`) |
| `--target-flash` | board with two SPI flashes: `primary` (default), `secondary`, or `both` |
| `-B, --bridge` | disable automatic bridge model detection by providing the bridge bitstream |

## Flash operations

| Option | Description |
|---|---|
| `--dump-flash` | dump flash to a file (with `--file-size` limits the window) |
| `--bulk-erase` | erase the whole flash (Lattice internal: `ALL` sector mask) |
| `--flash-sector` | sector selection — Lattice internal (`CFG`, `UFM`, `FEATURE`, `SRAM`, `ALL`), Altera MAX10 (`CFM0..2`, `UFM0..1`, comma-separated), Xilinx XCF |
| `--enable-quad` | enable quad mode for the SPI flash |
| `--disable-quad` | disable quad mode for the SPI flash |
| `--protect-flash` | protect an SPI flash area |
| `--unprotect-flash` | unprotect flash blocks |
| `--detect-external-flash` | detect and display the external SPI flash chip |
| `--list-flash` | list the supported SPI flash chips |

## Execution modes

| Option | Description |
|---|---|
| `--dfu` | DFU (USB) mode — `--altsetting` selects the DFU interface alternate setting |
| `--xvc` | use Xilinx Virtual Cable (client); `--ip`/`--port` address the endpoint |
| `--spi` | direct SPI mode (FTDI serial mode only) |
| `--pins` | pin config `TDI:TDO:TCK:TMS` (JTAG bitbang) or `MOSI:MISO:SCK:CS[:HOLDN:WPN]` (SPI) |
| `--ip` | IP address (XVC client, remote bitbang) |
| `--port` | XVC / remote bitbang TCP port (default 3721) |
| `--altsetting` | DFU interface alternate setting |

## Probe selection

| Option | Description |
|---|---|
| `--vid` | probe USB vendor ID |
| `--pid` | probe USB product ID |
| `--cable-index` | probe index (FTDI, CMSIS-DAP) |
| `--busdev-num` | select a probe by `bus_num:device_addr` |
| `--usb-serial-num` | USB iSerial — FTDI chip serial, or ESP32 iSerialNumber substring |
| `--ftdi-serial` | FTDI chip serial number (deprecated, use `--usb-serial-num`) |
| `--ftdi-channel` | FTDI channel 0-3 (maps to A-D) |
| `-d, --device` | device node, e.g. `/dev/ttyUSB0` (FTDI serial mode) |
| `--scan-usb` | scan USB and list connected probes |

## JTAG tuning

| Option | Description |
|---|---|
| `--freq` | requested JTAG frequency (Hz); the log reports the real frequency |
| `--invert-read-edge` | sample TDO on the negative edge (unstable links) |
| `--status-pin` | FTDI GPIO pin used as a status indicator (active low) |

## Cable firmware (Xilinx Platform Cable USB)

| Option | Description |
|---|---|
| `--probe-firmware` | firmware for the JTAG probe (`usbBlasterII` / `xilinxPlatformCableUsb`) |
| `--skip-probe-firmware-upload` | do not upload FX2 firmware; open an already-initialized `03fd:0008` |
| `--xpcu-direct-xp2-firmware` | for a cold `03fd:0013`, upload `xusb_xp2.hex` directly instead of `xusb_xp2_loader.hex` |

## Diagnostics and output

| Option | Description |
|---|---|
| `--detect` | detect the FPGA (add `-f` to also show the connected flash) |
| `--list-boards` | list supported boards |
| `--list-cables` | list supported cables |
| `--list-fpga` | list supported FPGAs |
| `-v, --verbose` | verbose output |
| `--verbose-level` | -1 quiet, 0 normal, 1 verbose, 2 debug |
| `--quiet` | no progress bar |
| `--force-terminal-mode` | emit progress-bar output even when not attached to a terminal |
| `-D, --read-dna` | read the FPGA DNA (Xilinx only) |
| `-X, --read-xadc` | read the XADC (Xilinx only) |
| `--read-register` | read a status register (Xilinx only) |
| `-V, --version` | print version |
| `-h, --help` | print help |

## Environment variables

| Variable | Effect |
|---|---|
| `OPENFPGALOADER_DATA_DIR` | absolute path of the data directory (the `share/openFPGALoader` folder with bridge bitstreams and XPCU firmware). Overrides the automatic search, which tries in order: `<exe dir>\share\openFPGALoader`, `<exe dir>\..\share\openFPGALoader` (portable layout), the working-directory variants, then the compiled-in install prefix |
| `OPENFPGALOADER_SOJ_DIR` | directory containing SPI-over-JTAG bridge bitstreams (default: the resolved data directory) |
| `OPENFPGALOADER_XUSB_FIRMWARE` | explicit XPCU boot firmware HEX path |
| `OPENFPGALOADER_XUSB_XLP_FIRMWARE` / `OPENFPGALOADER_XUSB_XP2_FIRMWARE` | explicit XPCU second-stage HEX paths |
| `OPENFPGALOADER_XPCU_CONTROL_BITBANG=1` | force the safe XPCU control-transfer JTAG path |
| `OPENFPGALOADER_XPCU_SKIP_FIRMWARE_UPGRADE=1` | skip second-stage FX2 reloads (see the [XPCU field notes](../debug/xpcu/windows-setup.md) for the full list) |

## Default behavior worth knowing

- Without `-f`, a loadable bitstream goes to **SRAM**.
- The file type is detected from the **extension**; pass `--file-type` to
  override (mandatory when reading from STDIN:
  `cat design.bit | openFPGALoader --file-type bit ...`).
- `--dump-flash` without `--file-size` dumps to the **end of the chip**
  when the flash model is known.
- Bridge bitstreams are searched in the installed data root, overridable by
  `OPENFPGALOADER_SOJ_DIR`.
