# Runtime flows

These flows describe the behavior that matters when debugging a failed
programming attempt. They are intentionally expressed in terms of the actual
classes and interfaces rather than an idealized service architecture.

## JTAG programming

```mermaid
sequenceDiagram
    participant U as Operator
    participant M as main.cpp
    participant J as Jtag
    participant A as JtagInterface adapter
    participant D as Vendor Device
    participant F as Parser / Flash service
    participant T as Target hardware

    U->>M: board/cable + bitstream + operation
    M->>M: Resolve board, cable, part, mode
    M->>J: Construct transport and JTAG engine
    J->>A: Reset TAP and detect chain
    A->>T: TMS/TDI clocks
    T-->>A: TDO IDCODE/scan data
    A-->>J: Chain IDs and IR lengths
    M->>D: Select driver from target IDCODE
    D->>F: Parse configuration or prepare flash data
    D->>J: Vendor instruction/data scans
    J->>A: Shift IR/DR with bypass padding
    A->>T: Program, verify, or read
    T-->>A: Status/TDO data
    A-->>M: Result and diagnostics
```

The important failure boundaries are:

| Boundary | Typical symptom | First place to inspect |
| --- | --- | --- |
| Cable discovery | No cable, permission error, wrong serial | Adapter constructor and platform dependencies in `src/cables/` |
| TAP reset or clocking | IDCODE is zero/unstable, scan never completes | `Jtag::go_test_logic_reset()`, clock frequency, read/write edge |
| Chain interpretation | Correct IDCODE at wrong index, trailing artifact | `Jtag::detectChain()`, `device_select()`, BYPASS bookkeeping |
| Vendor selection | Unsupported device or wrong programming sequence | `part.hpp`, feature flags, vendor dispatch in `main.cpp` |
| File parsing | Wrong size, header error, silent truncation | Matching parser in `src/parsers/` |
| Flash algorithm | Busy timeout, protection, verify mismatch | `SPIFlash`/`BPIFlash`, flash database, vendor-specific path |
| Runtime data | SOJ/BPI/firmware file not found | Install layout, `DATA_DIR`, `OPENFPGALOADER_SOJ_DIR` |

## Direct SPI

Direct SPI is selected by `--spi` or by a board whose communication mode is
`COMM_SPI`. The route bypasses the JTAG chain engine and uses a cable/device
implementation that satisfies `FlashInterface`, commonly FTDI SPI or a
direct Lattice/ICE40 path.

```mermaid
flowchart LR
    cli[main.cpp] --> direct[spi_comm or direct SPI device]
    direct --> flash[FlashInterface]
    flash --> algo[SPIFlash algorithms]
    algo --> adapter[FTDI/SPI adapter]
    adapter --> memory[Configuration flash]
```

Because no JTAG IDCODE is required, direct SPI failures should be diagnosed
through cable enumeration, chip-select wiring, JEDEC identification, and
flash geometry before investigating FPGA vendor drivers.

## DFU

DFU is selected by `--dfu` or a `COMM_DFU` board entry. The DFU adapter owns
USB control transfers and alternate-setting selection. It is a different
transport contract from `JtagInterface`; do not add DFU-specific state to the
JTAG engine just to share a command-line path.

```mermaid
flowchart TD
    cli[main.cpp] --> dfu[DFU cable adapter]
    dfu --> usb[USB DFU control transfers]
    usb --> boot[FPGA board bootloader]
    boot --> config[Configuration memory or FPGA fabric]
```

## XVC

XVC has two forms in the project: an XVC client cable adapter and, when
compiled, an XVC server mode. The server accepts remote scan requests and
routes them through a local cable adapter; the client makes a remote endpoint
look like a `JtagInterface` to the normal JTAG engine.

```mermaid
flowchart LR
    host[Remote XVC client] --> server[XVC server mode]
    server --> local[JtagInterface]
    local --> probe[Local cable]
    probe --> target[Target JTAG chain]
```

## Bitstream and flash lifecycle

The parser and flash layers have separate responsibilities:

1. The parser validates the input format and extracts the configuration
   payload.
2. The vendor driver chooses whether the payload targets SRAM, internal flash,
   or an external configuration flash.
3. The flash service discovers or accepts the flash type, applies protection
   policy, erases the required region, writes data, and optionally verifies it.
4. The vendor driver restores the target to the requested reset/run state.

Do not “fix” a parser by changing a flash offset, or fix a vendor instruction
sequence by changing generic flash geometry. Those are different ownership
boundaries and should be debugged separately.
