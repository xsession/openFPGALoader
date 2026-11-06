# Quick start

Get your first FPGA programmed in two commands. This page assumes
openFPGALoader is installed — see [Installation](install.md) if it is not.

## 1. Install

Packages are available for Linux distributions, Windows (MSYS2), and macOS:

- *Debian/Ubuntu*: `sudo apt install openfpgaloader`
- *Arch Linux*: `sudo pacman -S openfpgaloader`
- *Fedora*: `sudo dnf copr enable mobicarte/openFPGALoader && sudo dnf install openFPGALoader`
- *MSYS2*: `pacman -S mingw-w64-ucrt-x86_64-openFPGALoader`
- *macOS*: `brew install openfpgaloader`

More scenarios (Guix, source builds, Windows cross-compilation, udev rules)
are covered in [Installation](install.md).

## 2. Program a development board

Replace `my_fpga_board` with any board from the
[board compatibility list](../compatibility/boards.md) (or
`openFPGALoader --list-boards`). Choose the memory you want to target:

```bash
openFPGALoader -b my_fpga_board my_bitstream.bit  # program SRAM (volatile)
openFPGALoader -b my_fpga_board -f my_bitstream.bit # program flash (persistent)
```

> **Note**
> When a bitstream is compatible with both, the default is to load it into
> SRAM. Add `-f` to write flash.

## 3. Program an FPGA with an external cable

If your FPGA has no built-in programmer, or you prefer an external cable,
select a cable from the [cable compatibility list](../compatibility/cables.md)
(or `openFPGALoader --list-cables`):

```bash
openFPGALoader -c my_cable my_bitstream.bit    # program SRAM
openFPGALoader -c my_cable -f my_bitstream.bit # program flash
```

> **Note**
> On some boards (for example Digilent adapters) the converter's signals do
> not connect directly to the FPGA — in those cases `-c` is required even
> when a board name exists.

> **Hint**
> FTDI cable users: `-d` selects a specific FTDI device when several are
> connected:
>
> ```bash
> openFPGALoader -d /dev/ttyUSB0
> ```
>
> Without `-d`, openFPGALoader opens the first FTDI adapter it finds.

## 4. Verify and continue

```bash
openFPGALoader -b my_fpga_board --detect -f -v   # show chain + flash
```

If something fails, start with [Troubleshooting](troubleshooting.md).
To understand what the tool is doing under the hood, read
[How it works: overview](../how-it-works/overview.md).
