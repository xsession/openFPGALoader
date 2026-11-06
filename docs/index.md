# openFPGALoader

openFPGALoader is a universal, cross-platform command-line utility for
programming FPGAs (and some CPLDs) from Linux, Windows, macOS, and OpenBSD.
It talks to the FPGA through a JTAG cable — or, where needed, directly over
SPI, DFU, or a virtual JTAG link — and supports hardware from Xilinx/AMD,
Intel/Altera, Lattice, Gowin, Efinix, Anlogic, and Cologne Chip.

```bash
openFPGALoader -b arty arty_bitstream.bit        # load the volatile SRAM
openFPGALoader -b arty -f arty_bitstream.bit     # write the persistent flash
openFPGALoader -c cmsisdap fpga_bitstream.bit    # select a cable directly
```

New here? The [quick start](guide/quickstart.md) gets you flashing in
minutes. Not sure whether your hardware is supported? Check the
[boards](compatibility/boards.md), [FPGAs](compatibility/fpgas.md), and
[cables](compatibility/cables.md) compatibility lists.

## How to read these docs

| If you are... | Start here |
|---|---|
| Programming a board for the first time | [Quick start](guide/quickstart.md) → [Usage tutorial](guide/usage.md) |
| Installing from a package or source | [Installation](guide/install.md) |
| Looking for a command or option | [CLI reference](guide/cli-reference.md) |
| Wondering *how* the tool actually works | [How it works](how-it-works/overview.md) |
| Using a specific FPGA family | [Hardware guides](hardware/xilinx.md) (Xilinx, Lattice, Intel, Gowin, Efinix, Anlogic, Cologne Chip) |
| Fighting a cable, flash, or bridge problem | [Troubleshooting](guide/troubleshooting.md) → [Field notes](debug/soj-review.md) |
| Adding a device, flash, or cable to the source | [Contributing](development/contributing.md) → [Adding support](development/adding-support/fpga-and-flash.md) |
| Maintaining the build, CI, or releases | [Architecture](development/architecture/overview.md) → [Deployment](development/deployment/overview.md) |

## The short version

One command, four possible execution paths:

1. **JTAG** (the default): detect the JTAG chain, identify the FPGA by its
   IDCODE, dispatch to the vendor driver, and program SRAM or flash — using a
   small *SPI-over-JTAG bridge* bitstream when the FPGA cannot reach its
   configuration flash directly.
2. **Direct SPI**: the cable is wired straight to the flash (iCE40, some
   Efinix and GateMate setups). No JTAG chain is involved.
3. **DFU**: USB Device Firmware Upgrade for boards with a USB bootloader
   (ULX3S, OrbTrace).
4. **XVC**: Xilinx Virtual Cable — remote JTAG over TCP/IP, as client or
   server.

The [How it works](how-it-works/overview.md) section explains each path from
the operator's point of view down to the protocol, with diagrams.

## Community

The project is maintained at
[github.com/xsession/openFPGALoader](https://github.com/xsession/openFPGALoader)
and has a dedicated channel: [#openFPGALoader on libera.chat](https://web.libera.chat/#openFPGALoader).
