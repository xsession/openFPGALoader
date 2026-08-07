# Device Support Conversation

Date: 2026-07-13

## Requested devices

- Xilinx Virtex-4 `XC4VFX100-11FFG1517C`
- Lattice MachXO3L `LCMXO3L-9400E-5BG256C`
- Lattice MachXO3LF `LCMXO3LF-9400C-5BG256C`
- Intel Arria 10 SoC

## Conversation summary

The initial request was to add the Virtex-4 and MachXO3 devices. The two
MachXO3 9400 variants were already present in the device table and use the
existing MachXO3 programming flow.

Virtex-4 `XC4VFX100` detection and volatile JTAG programming support were
added with IDCODE `0x01ee4093`. This device has two PowerPC blocks and uses a
14-bit JTAG instruction register, so a Virtex-4-specific instruction map was
added instead of using the one-byte default map.

The package descriptor `4vfx100ff1517.txt` was then supplied for SPI-over-JTAG
support. It confirms the dedicated CCLK pin (`W20`) and the package pin names,
but it is not a PCB netlist and therefore does not identify the ordinary FPGA
I/O pins connected to the external SPI flash. The Virtex-4 HDL paths for
`BSCAN_VIRTEX4` and `STARTUP_VIRTEX4` were added. Completing a safe UCF and
building the bridge still requires the board's flash model and its CS, MOSI,
MISO, WP, and HOLD net-to-FPGA-pin connections, plus confirmation that the
flash clock is connected to CCLK.

The latest request was to add explicit Intel Arria 10 SoC support. Arria 10 SX
fabric IDCODEs were already listed under the generic Arria 10 family. They are
now identified explicitly as Arria 10 SoC devices and routed through a named
Arria 10 family in the Altera driver while preserving its existing RBF/SVF
programming path.

## Validation notes

- The earlier Xilinx and Lattice changes compiled and linked successfully.
- `--list-fpga` reported `xc4vfx100`, `LCMXO3L-9400E`, and
  `LCMXO3LF-9400C`.
- A build with Altera, Xilinx, and Lattice support completed successfully.
- `--list-fpga` reported all seven Arria 10 SX SoC densities.
- Virtex-4 SPI-over-JTAG cannot be safely completed from a package descriptor
  without the board-level flash wiring.

## Xilinx Platform Cable USB follow-up

The packaged Windows build loaded `xusb_emb.hex` successfully and reported the
expected FX2 firmware version `0x0404`, CPLD version `0x1200`, connected status,
and selected bulk endpoints `0x02`/`0x86`. The first accelerated JTAG transfer
command (`0xA6`) nevertheless timed out. The resulting IDCODE `0x78443190` was
corrupted scan data, not a new FPGA identifier.

The XPCU backend now encodes the full documented 24-bit accelerated-transfer
count and automatically falls back to the cable's older `0x30`/`0x38` control-
transfer JTAG protocol when `0xA6` is rejected. The fallback can be selected at
startup with `OPENFPGALOADER_XPCU_CONTROL_BITBANG=1`. A fresh Windows executable
and ZIP package were built successfully on 2026-07-13.
