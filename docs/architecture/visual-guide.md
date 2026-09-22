# Visual guide to openFPGALoader

This page is a diagram-first tour of the project. Read from top to bottom when
you are new to the codebase; jump directly to the relevant diagram when
debugging a board, cable, flash, or workflow problem.

The diagrams are intentionally tied to concrete implementation boundaries. The
names in brackets are the source locations that own the behavior.

## 1. One request, one target path

The whole product can be understood as a command-line request entering one of
four execution paths.

```mermaid
flowchart LR
    user["Operator or CI"] --> cli["openFPGALoader CLI\n[src/main.cpp]"]
    cli --> mode{"Select mode"}
    mode --> jtag["JTAG\n[protocols/jtag]"]
    mode --> spi["Direct SPI\n[FlashInterface]"]
    mode --> dfu["DFU\n[DFU adapter]"]
    mode --> xvc["XVC\n[client or server]"]
    jtag --> target["FPGA target"]
    spi --> target
    dfu --> target
    xvc --> target
```

**Teaching point:** board metadata and explicit flags select the path; the
selected path determines whether chain discovery is required.

## 2. How `main.cpp` routes a request

```mermaid
flowchart TD
    start["Parse command line"] --> list{"Is it a list command?"}
    list -->|"yes"| display["Display compiled catalogs"]
    list -->|"no"| resolve["Resolve board and cable"]
    resolve --> spi{"SPI mode?"}
    spi -->|"yes"| run_spi["Run direct SPI path"]
    spi -->|"no"| dfu{"DFU mode?"}
    dfu -->|"yes"| run_dfu["Run DFU path"]
    dfu -->|"no"| xvc{"XVC server?"}
    xvc -->|"yes"| run_xvc["Run XVC server"]
    xvc -->|"no"| run_jtag["Run JTAG path"]
```

The dispatch order is important: a board can imply SPI or DFU, while the
default path is JTAG. Changes to the mode decision should be reviewed against
the option parser, board catalog, and cable registry together.

## 3. JTAG programming at a glance

```mermaid
sequenceDiagram
    participant O as Operator
    participant M as main.cpp
    participant J as Jtag
    participant A as JtagInterface
    participant D as Device driver
    participant P as Parser
    participant H as Hardware

    O->>M: Board, cable, file, operation
    M->>J: Construct selected adapter
    J->>A: Reset TAP and detect chain
    A->>H: TMS, TDI, TCK
    H-->>A: TDO IDCODE and scan data
    A-->>J: Device IDs and IR lengths
    M->>D: Select driver from IDCODE
    D->>P: Parse configuration file
    P-->>D: Normalized payload
    D->>J: Vendor IR and DR scans
    J->>A: Buffered physical transfers
    A->>H: Program or verify target
    H-->>M: Status and diagnostics
```

**Teaching point:** the vendor driver owns device semantics, while `Jtag` owns
TAP state, scan framing, and chain padding.

## 4. JTAG discovery and target selection

```mermaid
flowchart TD
    reset["TAP reset"] --> idcode["Read IDCODE stream"]
    idcode --> split["Split devices by IR length and scan order"]
    split --> lookup["Look up IDCODE in part catalog"]
    lookup --> known{"Known device?"}
    known -->|"yes"| chain["Add device to logical chain"]
    known -->|"no"| misc["Use user-supplied misc-device or report unknown"]
    chain --> select["Select --index-chain target"]
    misc --> select
    select --> driver["Dispatch vendor Device implementation"]
```

The logical chain is not always identical to the electrical chain. Passive
devices, bridge CPLDs, and trailing scan artifacts may contribute bits without
being programming targets.

## 5. Why BYPASS alignment matters

```mermaid
flowchart LR
    lead["Leading device\nBYPASS = 1 DR bit"] --> target["Selected FPGA\nreal DR length"]
    target --> trail["Trailing device\nBYPASS = 1 DR bit"]
    scan["Requested target scan"] --> pad["Jtag adds IR/DR padding"]
    pad --> physical["Physical TDI/TDO stream"]
    lead --> physical
    target --> physical
    trail --> physical
```

When the selected target is in the middle of a chain, the requested data must
be surrounded by the correct BYPASS bits. A one-bit mistake can look like a
vendor failure, a flash timeout, or a bad read edge even though the root cause
is chain alignment.

Relevant implementation state lives in `Jtag`: device order, IR lengths,
leading/trailing padding, selected index, and trailing scan-artifact tracking.

## 6. Bitstream parsing is separate from programming

```mermaid
flowchart LR
    file["User file"] --> detect["Extension or --file-type"]
    detect --> parser["Raw, HEX, MCS, JED, POF, or vendor parser"]
    parser --> normalize["Header, compression, byte/word order"]
    normalize --> payload["Normalized configuration payload"]
    payload --> operation{"Operation"}
    operation --> sram["SRAM programming"]
    operation --> flash["Flash programming"]
```

**Boundary rule:** a parser should transform input data and report malformed
input. It should not open a cable, choose a JTAG device, or silently change a
flash offset.

## 7. Flash programming lifecycle

```mermaid
stateDiagram-v2
    [*] --> SelectPath
    SelectPath --> Identify: Internal or external flash
    Identify --> ResolveModel: JEDEC or explicit model
    ResolveModel --> CheckProtection
    CheckProtection --> Unprotect: User requested or required
    CheckProtection --> Erase: Writable
    Unprotect --> Erase
    Erase --> Write
    Write --> Verify: --verify
    Write --> Reset: No verification
    Verify --> Reset: Match
    Verify --> Error: Mismatch or timeout
    Reset --> [*]
    Error --> [*]
```

The vendor driver exposes the device-specific flash path. `SPIFlash` or
`BPIFlash` then performs generic memory operations using the flash database and
the `FlashInterface` contract.

## 8. Direct SPI versus SPI-over-JTAG

```mermaid
flowchart TD
    request["Flash request"] --> path{"How is flash exposed?"}
    path -->|"Direct SPI"| ftdi["FTDI or direct SPI adapter"]
    path -->|"SPI over JTAG"| bridge["Load SOJ bridge bitstream"]
    bridge --> jtag["JTAG scans through bridge"]
    ftdi --> spi["FlashInterface"]
    jtag --> spi
    spi --> algorithm["SPIFlash algorithm"]
    algorithm --> memory["Configuration flash"]
```

The two paths share flash concepts but not necessarily the same timing,
buffering, or error signatures. Diagnose the exposure path first.

## 9. SOJ: the bridge inside the JTAG path

```mermaid
sequenceDiagram
    participant M as openFPGALoader
    participant J as JTAG adapter
    participant S as SOJ bridge FPGA logic
    participant F as SPI flash

    M->>J: Configure target JTAG path
    M->>J: Load family-specific SOJ bitstream
    J->>S: Shift bridge configuration
    S-->>J: Bridge ready or status
    M->>J: Send SPI command as JTAG scan data
    J->>S: TDI/TMS/TCK transport
    S->>F: SPI clock, command, address, data
    F-->>S: JEDEC/status/read data
    S-->>J: TDO response bits
    J-->>M: Flash result
```

SOJ failures can therefore originate in the asset, data lookup, JTAG framing,
adapter buffering, bridge RTL, flash timing, or response alignment. The
[SOJ review](../SOJ_REVIEW.md) is the specialist trace-level companion to
this diagram.

## 10. Cable abstraction layers

```mermaid
flowchart TD
    device["Vendor Device"] --> high["Jtag or FlashInterface"]
    high --> contract["Protocol contract"]
    contract --> usb["USB / HID adapter"]
    contract --> gpio["GPIO adapter"]
    contract --> remote["Remote bitbang or XVC"]
    contract --> vendor_probe["Vendor probe adapter"]
    usb --> host["Operating system libraries"]
    gpio --> host
    remote --> network["Network endpoint"]
    vendor_probe --> host
```

The protocol contract is the stability boundary. A new cable should normally
implement an adapter instead of adding USB or GPIO conditionals to a vendor
driver.

## 11. Compile-time capability gates

```mermaid
flowchart LR
    option["CMake option"] --> source["Conditional source inclusion"]
    option --> library["Dependency discovery"]
    source --> binary["Compiled capability"]
    library --> binary
    binary --> catalog["Runtime catalog entries"]
    catalog --> cli["--list-* and selection behavior"]
```

This is why a source file existing in `src/cables/` or `src/vendors/` does not
guarantee that every package supports it. Review configuration, compilation,
and runtime advertisement as one change.

## 12. Build and package flow

```mermaid
flowchart TD
    source["Git source and submodules"] --> configure["CMake configure"]
    deps["Platform dependencies"] --> configure
    flags["Vendor and cable options"] --> configure
    configure --> compile["Compile and link"]
    compile --> stage["Install staging tree"]
    assets["SOJ, BPI, firmware, rules"] --> stage
    stage --> linux["Linux archive"]
    stage --> mac["macOS archive"]
    stage --> win["Windows ZIP"]
```

**Teaching point:** runtime assets are part of the product. An executable
without its SOJ/BPI data or required DLLs is an incomplete deployment.

## 13. CI gates and release artifacts

```mermaid
flowchart LR
    change["Push or pull request"] --> docs["MkDocs strict build"]
    change --> source_test["Parser regression tests"]
    change --> native["Linux and macOS builds"]
    change --> windows["MSYS2 and cross Windows builds"]
    docs --> release{"Release conditions"}
    source_test --> release
    native --> release
    windows --> release
    release --> artifacts["Archives, checksums, documentation"]
```

The matrix catches packaging and CLI regressions, but physical cable timing,
probe firmware, signal integrity, and every FPGA/flash combination still need
hardware validation.

## 14. Deployment topology

```mermaid
flowchart TD
    host["Developer or CI host"] --> executable["openFPGALoader executable"]
    executable --> shared["Shared libraries or DLLs"]
    executable --> data["Installed transport data"]
    executable --> adapter["Cable or remote endpoint"]
    adapter --> target["FPGA board and JTAG chain"]
    target --> flash["Configuration flash"]
```

For the Windows cross-build, the host-side container must be Linux-based
because the build image is Alpine Linux. The resulting executable is Windows
portable, but the build environment is not a Windows container.

## 15. Troubleshooting decision tree

```mermaid
flowchart TD
    fail["Programming failed"] --> detect{"Can the cable be detected?"}
    detect -->|"no"| access["Check driver, permissions, USB, firmware"]
    detect -->|"yes"| chain{"Is JTAG IDCODE stable?"}
    chain -->|"no"| signal["Lower frequency; check wiring and read edge"]
    chain -->|"yes"| target{"Is target index/vendor correct?"}
    target -->|"no"| alignment["Inspect chain order and BYPASS padding"]
    target -->|"yes"| file{"Does parser accept the file?"}
    file -->|"no"| format["Check format, compression, and byte order"]
    file -->|"yes"| flash{"Does flash identify and verify?"}
    flash -->|"no"| memory["Check protection, model, erase, timing"]
    flash -->|"yes"| hardware["Investigate target-specific sequence or asset"]
```

This prevents jumping directly to a vendor driver when the failure is actually
transport discovery, scan alignment, input parsing, or flash protection.

## 16. Contributor change routing

```mermaid
flowchart TD
    change["What are you changing?"] --> board{"Board or default?"}
    change --> cable{"Cable or transport?"}
    change --> vendor{"Vendor or FPGA sequence?"}
    change --> parser{"File format?"}
    change --> flash{"Flash model or algorithm?"}
    change --> build{"Build or package?"}
    board --> board_files["board.hpp + compatibility docs"]
    cable --> cable_files["cable.hpp + src/cables + CMake"]
    vendor --> vendor_files["part.hpp + src/vendors + hardware note"]
    parser --> parser_files["src/parsers + regression fixture"]
    flash --> flash_files["spiFlashdb + flash tests"]
    build --> build_files["CMake + deploy + workflow"]
```

After the focused change, run the broad gates: compatibility generation,
strict MkDocs, parser tests, build/smoke tests, and the relevant physical
fixture if the behavior is hardware-dependent.
