# openFPGALoader

openFPGALoader is a universal utility for programming FPGAs from Linux,
Windows, macOS, and OpenBSD. It supports boards, cables, and devices from
Xilinx/AMD, Intel/Altera, Lattice, Gowin, Efinix, Anlogic, and Cologne Chip.

## Quick start

```bash
openFPGALoader -b arty arty_bitstream.bit       # load SRAM
openFPGALoader -b arty -f arty_bitstream.bit     # write flash
openFPGALoader -c cmsisdap fpga_bitstream.bit   # choose a cable directly
```

Use `openFPGALoader --list-boards` and
`openFPGALoader --list-cables` to inspect the locally supported hardware.

## Find your hardware

- [FPGA compatibility](compatibility/fpga.md)
- [Board compatibility](compatibility/board.md)
- [Cable compatibility](compatibility/cable.md)

## Continue

- Start with the [first-steps guide](guide/first-steps.md).
- Install from a package or [build from source](guide/install.md).
- Use [troubleshooting](guide/troubleshooting.md) when detection or flashing
  fails.
- Read the [SOJ/JTAG review](SOJ_REVIEW.md) for SPI-over-JTAG diagnostics and
  known hardware-dependent alignment cases.

The project is maintained at
[github.com/xsession/openFPGALoader](https://github.com/xsession/openFPGALoader).
