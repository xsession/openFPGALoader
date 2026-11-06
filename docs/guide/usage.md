# Usage tutorial

This tutorial walks through the operations you will actually perform, in
the order you learn them. Each section ends with the exact command to run.
The full option set is in the [CLI reference](cli-reference.md); this page
teaches the workflow.

> **Prerequisite**
> You have openFPGALoader installed (see [Installation](install.md)) and
> your cable is recognized. A quick sanity check:
>
> ```bash
> openFPGALoader --list-boards     # your board is in the list
> openFPGALoader --scan-usb        # your cable appears
> ```

## 1. Program the FPGA in SRAM (volatile)

The fastest operation: the design appears on the board immediately and
disappears with power.

```bash
openFPGALoader -b <board> <bitstream>
# e.g.
openFPGALoader -b arty impl/pnr/artix7.bit
```

`-m` is implied. If your board is not in the catalog but your cable and
FPGA part are, use the cable and part directly:

```bash
openFPGALoader -c <cable> --fpga-part xc7a35tcsg324 design.bit
```

## 2. Program the flash (non-volatile)

Same image, written to the persistent configuration flash so the board boots
it on power-up:

```bash
openFPGALoader -b <board> -f <bitstream>
```

What happens under the hood (see [flash programming](../how-it-works/flash-programming.md)
for the full lifecycle): the tool identifies the flash by its JEDEC ID,
unprotects and erases the needed region, programs page by page, and
relocks. On Xilinx boards this also means loading a temporary
[SPI-over-JTAG bridge](../how-it-works/spi-over-jtag.md) into SRAM first.

> **Note**
> File format matters per vendor — the right extension comes from your
> synthesis tool: Xilinx `.bit`/`.bin`/`.mcs`, Lattice `.bit`/`.jed`/`.mcs`,
> Intel `.svf`/`.rbf`/`.rpd`/`.pof`, Gowin `.fs`, Efinix `.hex`/`.bin`,
> Anlogic `.bit`/`.svf`, iCE40 `.bin`. See the
> [hardware guides](../hardware/xilinx.md) for family-specific recipes.

## 3. Verify the write

Flash writes are verified explicitly:

```bash
openFPGALoader -b <board> -f --verify <bitstream>
```

For SRAM, `--verify` re-reads the fabric through the vendor readback where
supported.

## 4. Dump the flash

Back up or inspect what is in the flash:

```bash
# whole flash (size auto-detected from the JEDEC database)
openFPGALoader -b <board> --dump-flash flash_dump.bin

# from an offset to the end
openFPGALoader -b <board> --dump-flash -o 0x100000 tail.bin

# an explicit window
openFPGALoader -b <board> --dump-flash -o 0x1000 --file-size 0x2000 window.bin
```

For Lattice MachXO2/3, `--dump-flash` reads the **internal** NVCM (CFG pages
followed by UFM pages) instead of an external chip; for Altera MAX10 it
reads the internal flash.

## 5. Erase the flash

```bash
openFPGALoader -b <board> --bulk-erase
```

> **Warning**
> On Lattice MachXO2/MachXO3, a plain `--bulk-erase` uses the `ALL` mask,
> which also erases the **feature row** and can make the board look bricked.
> Prefer a scoped erase:
>
> ```bash
> openFPGALoader -b <board> --bulk-erase --flash-sector CFG
> ```
>
> See the [recovery field note](../debug/machxo3-recovery-after-bulk-erase.md)
> if it already happened.

For Altera MAX10 internal flash, select sectors explicitly:

```bash
openFPGALoader -b step-max10_v1 -f --flash-sector UFM1,CFM0,CFM2 design.pof
```

## 6. Detect: what is the tool seeing?

```bash
openFPGALoader -b <board> --detect            # FPGA (and, with -f, the flash)
openFPGALoader -c <cable> --detect -v         # verbose: chain, IDCODE, versions
openFPGALoader --detect-external-flash        # external flash model only
openFPGALoader --list-flash                   # supported flash chips
```

`--detect` is the single most useful diagnostic: it runs chain detection
and reports IDCODE, family, model, and IR length (plus the flash model when
`-f` is present) without changing anything.

## 7. Write raw data at an arbitrary flash offset

Useful for updating soft-core firmware or data areas without touching the
bitstream:

```bash
openFPGALoader -b <board> -f -o 0x10000 firmware.bin
```

Works with Xilinx, Lattice (ECP5/nexus/ice40), Anlogic, and Efinix external
flash.

## 8. Boards with two flash chips

Some UltraScale boards (KCU105/116, VCU118/128, ...) have primary and
secondary SPI flashes:

```bash
# detect both
openFPGALoader -b kcu105 -f --target-flash primary --detect
openFPGALoader -b kcu105 -f --target-flash secondary --detect

# program only the secondary with arbitrary data
openFPGALoader -b vcu118 -f --target-flash secondary --bitstream data.bin

# program both (SPIx8, two .mcs images from Vivado)
openFPGALoader -b vcu118 -f --target-flash both \
  --bitstream impl/pnr/design_primary.mcs \
  --secondary-bitstream impl/pnr/design_secondary.mcs
```

## 9. Multiple devices on one JTAG chain

When the FPGA shares its chain with other JTAG devices (e.g. a Spartan-3
with an independent XCF PROM, or a bridge CPLD), select the target by its
zero-based index:

```bash
openFPGALoader -c <cable> --index-chain 1 -f image.mcs
```

Run `--detect -v` first to see the numbered chain. If a passive device
disturbs the index, absorb it with `--misc-device <idcode,irlen,name>`.

## 10. A complete example workflow (Xilinx Artix-7)

```bash
# 1. see the chain and the flash
openFPGALoader -b nexys_a7_50 --detect -f -v

# 2. load the design and watch it run
openFPGALoader -b nexys_a7_50 -m impl/pnr/design.bit

# 3. back up the existing flash
openFPGALoader -b nexys_a7_50 --dump-flash backup.bin

# 4. write the new bitstream to flash, verified
openFPGALoader -b nexys_a7_50 -f --verify impl/pnr/design.bit

# 5. power-cycle the board — it now boots from flash
```

## Where to go next

- [Advanced usage](advanced-usage.md) — STDIN bitstreams, FT231X/FT232R
  bitbang pins, negative-edge sampling, alternative bridge directories.
- [CLI reference](cli-reference.md) — every option, grouped.
- [Troubleshooting](troubleshooting.md) — when a step above does not work.
- [How it works](../how-it-works/overview.md) — what each step does inside.
