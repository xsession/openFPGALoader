# How it works: JTAG transport

JTAG (IEEE 1149.1) is the default and most expressive path in
openFPGALoader: it can identify every device on a chain, configure the FPGA
in SRAM, and — through a bridge — reach the configuration flash. This page
explains the JTAG model the tool implements, so you can read its output and
diagnose failures.

## The TAP and the four signals

Every JTAG device contains a Test Access Port (TAP) driven by four signals:

| Signal | Role |
|---|---|
| **TCK** | clock — all transitions happen on TCK edges |
| **TMS** | TAP state select — a sequence of TMS values moves the TAP between states |
| **TDI** | data in — bits are shifted into the device on TCK edges |
| **TDO** | data out — the device shifts bits out on TCK edges |

The TAP state machine has 16 states; the three that matter in practice are
**Test-Logic-Reset** (entered by holding TMS high for five clocks),
**Shift-DR** (data register shifting), and **Shift-IR** (instruction
register shifting). The `Jtag` engine (`src/protocols/jtag.cpp`) owns this
state bookkeeping; cable adapters only have to deliver accurate
TMS/TDI/TCK waveforms and sample TDO.

## IDCODE: how devices identify themselves

Each device stores a 32-bit **IDCODE** in its data register. Shifting it out
is how the chain is discovered:

```text
 31      28 27          20  19        5 4      1 0
┌──────────┬───────────────┬───────────┬───┐
│ Version  │  Part number  │Manufacturer│ '1│
└──────────┴───────────────┴───────────┴───┘
```

Bits are shifted out **LSB first**, so the first byte you read is the least
significant. The manufacturer ID (bits 4:1) is the quick fingerprint:

| Manufacturer | ID (bits 4:1) | LSB-first low byte |
|---|---|---|
| Xilinx/AMD | 0x049 | `0x93` |
| Lattice | 0x021 | `0x43` |
| Intel/Altera | 0x06E | `0x6E` |
| Gowin | 0x40D | `0x1B` |
| Efinix | 0x53C | `0x35` |
| Anlogic | 0x61A | `0x35` |

openFPGALoader compares the full 32-bit value against the part catalog
(`src/utils/part.hpp`), which also stores each device's **IR length**
(instruction register length) — for example 6 bits for 7-series and
Spartan-6, 8 bits for ECP5/iCE40/MachXO, up to 14 bits for some Virtex-4.

> **Note**
> The 4 version bits (31:28) vary between silicon revisions of the same
> part. The tool masks them when classifying a device, which is why logs may
> show an IDCODE whose top nibble differs from the database value.

## Chain detection

A JTAG chain is a series of devices where one device's TDO feeds the next
device's TDI:

```text
TDO ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
    │  device 0    │  │  device 1    │  │ device N     │
──> │  (TMS,TCK,   ├──┤              ├──┤              │ ──> TDI
    │   TDI)       │  │              │  │              │
    └──────────────┘  └──────────────┘  └──────────────┘
```

Detection works as follows:

1. The TAP is reset (TMS high for five clocks).
2. The engine shifts out IDCODEs — first shifting the IDCODE instruction
   into each device's IR (when the IR length is known), then shifting DR
   bits out and reading TDO.
3. Shifting continues until a whole word of `1`s appears: that is the
   **terminator**, and it means the chain ends there.
4. The collected stream is split into individual 32-bit IDCODEs.
5. Each IDCODE is looked up in the part catalog; unknown words are reported
   as `misc` devices, or skipped when the user supplied `--misc-device`
   descriptors (`<idcode,irlen,name>`).

The result is a *logical* chain: an ordered list of devices with their
IDCODEs and IR lengths. The on-screen `found N devices` list is this
logical chain.

### The logical chain is not always the electrical chain

Bridge CPLDs, passive devices, and trailing scan artifacts can contribute
bits without being programming targets. The tool handles this with
`--index-chain` (select the Nth device, zero-based) and, where needed,
`--misc-device` to absorb a known non-FPGA word so the index of the real
target stays stable.

## BYPASS: keeping the rest of the chain quiet

While you program device *k* in the middle of a chain, bits still have to
pass *through* devices *0..k-1* and *k+1..N*. Those devices must be put into
**BYPASS** mode, in which each shifted TDI bit passes straight through TDO
(1 input bit = 1 output bit). The `Jtag` engine automatically inserts the
correct number of BYPASS bits **before** (leading devices) and **after**
(trailing devices) the selected target on every DR scan.

This is why a one-bit mistake in chain alignment can look like a vendor bug,
a flash timeout, or a bad read edge: the data reaches the wrong shift
register, or arrives shifted by a bit. The relevant state — device order, IR
lengths, leading/trailing padding, selected index — all lives in `Jtag`, and
is exactly what the [SOJ review](../debug/soj-review.md) audited.

## Instruction and data registers

- **Shift-IR** loads the instruction register, selecting which data
  register the next **Shift-DR** touches. Vendor drivers speak in
  *instruction names* (`IDCODE`, `USER1`, `ISC_ENABLE`, `JPROGRAM`,
  `JSTART`, ...); the vendor IR map (per family, e.g. 7-series vs Spartan-6
  vs Virtex UltraScale+) translates names to bit patterns. Families with
  IRs longer than 8 bits (some Virtex) use multi-byte LSB-first patterns.
- **Shift-DR** moves the actual data — IDCODE words, SRAM configuration
  bits, or (for SPI-over-JTAG) bit-reversed SPI command/address/data bytes.

## The cable adapters

The `JtagInterface` contract (`src/protocols/jtagInterface.hpp`) is the
stability boundary between the JTAG engine and the physical world. An
adapter turns "shift these bits" into real hardware traffic:

| Adapter | Technology | Notes |
|---|---|---|
| FTDI MPSSE (`ftdiJtagMPSSE`) | FT2232/FT4232/HS2/HS3 bit-bang engine | Digilent, bus blaster, most FTDI boards |
| FTDI bitbang (`ftdiJtagBitbang`) | FT231X/FT232R GPIO bit-banging | `--pins TDI:TDO:TCK:TMS` required |
| Xilinx Platform Cable USB (`xilinxPlatformCableUSB` + `fx2_ll`) | Cypress FX2 microcontroller + CPLD | volatile FX2 firmware upload, accelerated bulk transfers, control-transfer fallback — see [XPCU field notes](../debug/xpcu/technical-reproduction.md) |
| CMSIS-DAP (`cmsisDAP`) | DAPLink protocol | LPC-Link2, Sipeed SLogic, i5/i9 boards |
| J-Link (`jlink`) | SEGGER protocol | BASE/PRO probes |
| USB-Blaster I/II/III (`usbBlaster`) | Altera cables | FX2 or FT245 based |
| DFU (`dfu`) | USB DFU | non-JTAG path, see [alternate paths](alternate-paths.md) |
| Bitbang GPIO (`libgpiod`, `jetsonNanoJtagBitbang`) | host GPIO | Linux only |
| Remote bitbang (`remoteBitbang_client`) | OpenOCD remote_bitbang over TCP | |
| XVC client (`xvc_client`) | Xilinx Virtual Cable over TCP/IP | see [alternate paths](alternate-paths.md) |
| SVF (`svf_jtag`) | SVF script playback | |
| CH347 / CH552 / Anlogic / ESP32-S3 / GWU2X | vendor USB-JTAG bridges | |

Adapters differ in **buffering** (how many bits are batched per USB
transaction), **clocking** (requested vs real frequency — the log prints
both), and **edge sampling** (`--invert-read-edge` switches TDO sampling to
the falling edge for unstable links). Changing batching changes both
performance and electrical timing, so adapter behavior is part of the
hardware surface.

## Frequency and stability

`--freq` requests a JTAG clock in Hz. Every adapter reports the *real*
achieved frequency in the log; several cables (and the XPCU control
fallback in particular) run far slower than the requested value. If IDCODEs
come out unstable or zero:

1. lower the frequency (`--freq 100000` is a good probe speed),
2. try `--invert-read-edge`,
3. check wiring, ground reference, and competing drivers on the chain,
4. on Windows XPCU, see the [XPCU field notes](../debug/xpcu/windows-setup.md).

## Reading a detection log

A healthy detection prints, in order:

```text
Jtag frequency : requested 6.00MHz    -> real 6.00MHz
found 1 devices
index 0:
    idcode 0x04028093
    manufacturer xilinx
    family spartan6
    model  xc6slx45T
    irlength 6
```

Anything before `found N devices` belongs to the cable adapter; anything
after belongs to the JTAG engine and the part catalog. Use that split to
decide whether a failure is a *transport* problem or a *chain* problem.

## Further reading

- [Flash programming](flash-programming.md) — what happens after the chain is detected.
- [SPI-over-JTAG bridges](spi-over-jtag.md) — how SRAM programming doubles as flash access.
- [Visual guide — chain diagrams](../development/architecture/visual-guide.md) — discovery and BYPASS alignment, diagram-first.
- [Runtime flows — failure boundaries](../development/architecture/runtime-flows.md) — which class to inspect per symptom.
