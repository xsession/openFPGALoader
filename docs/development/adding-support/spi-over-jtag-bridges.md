# SPI-over-JTAG bridges: adding new parts

This page documents how to build SPI-over-JTAG (SOJ) bridge bitstreams for
new FPGA parts. The sources live in
[`src/transport_db/spiOverJtag/`](https://github.com/xsession/openFPGALoader/tree/master/src/transport_db/spiOverJtag)
(the source README in that directory is the canonical copy for contributors
working in the tree). For how bridges work at runtime, read
[SPI-over-JTAG bridges — theory of operation](../../how-it-works/spi-over-jtag.md).

## Why bridges exist

*Lattice ECP3/ECP5*, *Gowin GW2/GW5*, and *Cologne Chip GateMate* have an
internal interface to access external SPI flash via JTAG. In contrast,
accessing SPI flash on *Xilinx*, *Efinix* and *Altera Cyclone* FPGAs
requires loading a dedicated bitstream that bridges the JTAG and SPI
interfaces. `spiOverJtag` contains those bridge bitstreams.

> **Important notes**
>
> - `spiOverJtag` supports **single-wire mode only** (`MOSI`/`MISO`) for
>   maximum board compatibility.
> - Existing bridge bitstreams are versioned in the repository. Rebuild is
>   typically required only when adding support for a missing
>   FPGA model/package.

## Dependencies

- `make`
- `gzip`
- `python3`
- Python package: `edalize`
- Vendor toolchains (depending on target family):
  - Xilinx: Vivado and/or ISE
  - Altera: Quartus
  - Efinix: Efinity

Notes:

- `build.py` is used for **Xilinx** and **Altera** targets.
- `efinix_build.py` is used for **Efinix** targets.

## Build generalities

All steps below are performed from the `spiOverJtag` sub-directory.

Generated outputs:

- *Xilinx*: `spiOverJtag_<part>.bit.gz`
- *Efinix*: `spiOverJtag_efinix_<part>.bit.gz`
- *Altera*: `spiOverJtag_<part>.rbf.gz`

Build command examples:

```bash
# Xilinx Artix7 35t
make spiOverJtag_xc7a35t.bit.gz
# Altera Cyclone10CL 016
make spiOverJtag_10cl016484.rbf.gz
# Efinix Trion T13 F256
make spiOverJtag_efinix_t13f256.bit.gz
```

Clean temporary/build files:

```bash
make clean
```

## Package-specific bitstreams

Some boards within the same FPGA family/size use different SPI flash pin
mappings depending on the device package. When the SPI pins differ between
packages, `build.py` must produce a distinct bitstream per package rather
than aliasing one build to every package via symlinks.

To build a package-specific bitstream, pass the full device name
(device + package) to `make`. Example for the QMTech XC7A100T (FGG676)
core board:

```bash
make spiOverJtag_xc7a100tfgg676.bit.gz
```

This consumes `constr_xc7a_fgg676.xdc` and produces a bitstream wired to
the FGG676 SPI dedicated configuration pins (C8/B19/A18/B18/A19).

## Add support for a new device

### 1. Register the part in `Makefile`

Add the short part name into the appropriate list:

- `XILINX_PARTS`
- `ALTERA_PARTS`
- `EFINIX_PARTS`

### 2. Update build script mappings

For *Xilinx* (`build.py`), the approach depends on family:

- for `Artix`, `Kintex 7` and `Spartan 7`, size and packages are provided
  via the `packages` dict;
- for other devices, `pkg_name` (Vivado) or `tool_options` (ISE) must be
  updated to add the relationship between FPGA model/size/package and
  device/package.

For *Altera* (`build.py`): the `full_part` dict must be updated to match the
short format and a Quartus-compatible device name.

For *Efinix* (`efinix_build.py`): the script contains pin mapping data for
Trion and Titanium devices. Update `efinix_pinout` with a new sub-dict that
provides the pin name for each SPI IO, and add an entry for the new FPGA to
the `timing_models` dict. For Titanium devices, ensure the package mapping
exists in `efinix_pinout["Titanium"]` and the corresponding timing model
exists in `timing_models`.

### 3. Add constraints (if required)

`spiOverJtag` already contains many constraint files. Devices in the same
family with the same package usually share the same JTAG and SPI pinout
(for example, Xilinx Artix-7 devices with `ftg256` can reuse
`constr_xc7a_ftg256` regardless of device size). This step is only required
when a package is not yet available for a given FPGA family.

Add the constraint file expected by the mapping:

- *Xilinx Vivado*: `constr_<name>.xdc`
- *Xilinx ISE*: `constr_<name>.ucf`
- *Altera*: no explicit constraint file in this directory (handled by
  Quartus and the script flow)
- *Efinix*: generated/handled by `efinix_build.py`; update script data for
  new package pinouts when needed

### 4. Build the bitstream

Run the target build command and verify the compressed file is generated:

```bash
# Xilinx/Efinix
make spiOverJtag_<new-part>.bit.gz
# or for Altera
make spiOverJtag_<new-part>.rbf.gz
```

At the end of the tool execution, a new bitstream should be present in the
current directory.

> **Note**
> The directory `tmp_<part>/` (or `tmp_efinix_<part>/`) is the working
> directory and may be removed after bitstream generation.

## Installing the result

Copy the generated file to the installation share directory with the
naming convention `spiOverJtag_<part>.bit.gz` (or `bpiOverJtag_<part>`),
where `<part>` matches the `device + package` string of the part catalog.
The binary finds it through the configured data directory, overridable with
`OPENFPGALOADER_SOJ_DIR`.
