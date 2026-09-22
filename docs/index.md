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

## Understand the project

The [architecture overview](architecture/index.md) explains how the command
line application selects a transport, discovers a JTAG chain, delegates to a
vendor driver, parses a configuration file, and programs SRAM or flash. The
[visual architecture guide](architecture/visual-guide.md) adds a diagram-first
walkthrough of the mode router, JTAG alignment, flash lifecycle, SOJ bridge,
build gates, deployment, troubleshooting, and contributor change paths. The
[C4 documentation](architecture/c4/context.md) turns that source map into
context, container, component, and deployment views.

For maintainers, the [source map](architecture/source-map.md),
[runtime flows](architecture/runtime-flows.md), and
[architecture rules](contributing/architecture-rules.md) are the quickest way
to find the right change point without treating the repository as a black box.

The project is maintained at
[github.com/xsession/openFPGALoader](https://github.com/xsession/openFPGALoader).
