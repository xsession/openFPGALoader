# Spartan-6 XPCU SPI Flash Debug Log

## Scope

This note collects the investigation history for external SPI flash access on a Spartan-6 target through `xilinxPlatformCableUsb_alt` / XPCU in `openFPGALoader`.

Target context:

- FPGA: `xc6slx45tfgg484`
- Board flash believed to be: `W25Q64`
- Expected JEDEC ID: `0xef4017`
- Cable path under test: `xilinxPlatformCableUsb_alt`
- Reference behavior: Xilinx iMPACT can see and use the flash successfully on the same hardware

## Main Symptom

JTAG chain detection works, the SPI-over-JTAG bridge bitstream loads into SRAM, but flash access through the bridge fails.

Typical failure signatures seen during the investigation:

- `Read ID failed`
- `SPI RDID raw bytes: 00 00 00 00 -> 0x00000000`
- `SPI RDID raw bytes: ff ff ff ff -> 0xffffffff`
- `SOJ version raw: 00 00 00 00 00 00 00`
- `SOJ version raw: ff ff ff ff ff ff 00`

Important interpretation:

- If `USER4` bridge version readout is dead, the issue is broader than SPI command formatting.
- That points to one of:
  - the bridge design not truly entering active user mode after SRAM configuration
  - XPCU transport returning USER scan data incorrectly
  - USER instruction / scan framing mismatch on Spartan-6

## Baseline Observations

- JTAG chain detection is good.
- The cable can identify the Spartan-6 device:
  - `idcode 0x04028093`
  - family `spartan6`
  - model `xc6slx45T`
  - IR length `6`
- Loading the bridge bitstream reports success.
- After bridge load, `done=1` is often reported.
- Despite that, `USER4` version probing can still return all-zero or all-`ff` data.

This means "bitstream loaded" is not the same as "bridge logic is alive and reachable through USER scans".

## User-Visible Commands Used Repeatedly

Primary detect command:

```powershell
.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --detect-external-flash --index-chain 0 -v -v
```

Other explored variants:

```powershell
.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --detect-external-flash --index-chain 0 --enable-quad
.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --fpga-part xc6slx45tfgg484 --bridge E:\Xilinx\14.7\ISE_DS\ISE\spartan6\data\xc6slx45t_spi.cor --detect-external-flash --index-chain 0
.\openFPGALoader.exe -c xilinxPlatformCableUsb --detect -v
```

Environment switches explored:

```powershell
$env:OPENFPGALOADER_XPCU_CONTROL_BITBANG="1"
$env:OPENFPGALOADER_XPCU_CONTROL_BITBANG="0"
$env:OPENFPGALOADER_XPCU_TDO_MASK="0x02"
```

Key observed behavior:

- `OPENFPGALOADER_XPCU_CONTROL_BITBANG=1` could break detection completely on some runs.
- `OPENFPGALOADER_XPCU_CONTROL_BITBANG=0` worked better for chain detection and generally behaved better on this hardware.

## Features Added During Investigation

### VS Code Support

Workspace `.vscode` files were added/updated so the project can be built and debugged from VS Code.

### Logging

Additional logging was added to make failures easier to diagnose:

- raw SPI JEDEC readback bytes
- flash-access preparation context
- bridge version raw and decoded data
- XPCU mode / mask retry messages

### Flash Listing

A `--list-flash` option was added to print supported flash names and IDs.

### External Flash Detection Option

A `--detect-external-flash` option was added.

### ISE `.cor` Bridge Import Support

Bridge loading was extended so direct Spartan-6 `.cor` files from ISE can be used, for example:

```text
E:\Xilinx\14.7\ISE_DS\ISE\spartan6\data\xc6slx45t_spi.cor
```

## Important Code Changes Already Made

### 1. Spartan-6 USER Instruction Mapping

Spartan-6 uses different USER3/USER4 opcodes than 7-series.

Added mapping:

- `USER1 = 0x02`
- `USER2 = 0x03`
- `USER3 = 0x1a`
- `USER4 = 0x1b`

This matches Xilinx Spartan-6 documentation.

### 2. Bridge Version Probe Uses USER4 Mapping

The bridge version probe was updated to use the family-specific `USER4` opcode from the IR map instead of a hardcoded assumption.

### 3. Generic Bridge Loading / `.cor` Support

Bridge loading was changed to support:

- `.bit`
- `.bit.gz`
- `.cor`

### 4. Invalid JEDEC Handling Tightened

SPI flash JEDEC ID reads now explicitly reject these invalid values:

- `0x000000`
- `0x00ffff`
- `0xffffff`

and report the raw bytes before failure.

### 5. XPCU Control-Bitbang Work

Low-level support was added for:

- forcing control-transfer JTAG mode
- selecting alternate TDO masks
- restoring the primary TDO mask
- a special USER scan capture mode

This was useful for experimentation, but forcing control-bitbang turned out not to be the best default for this hardware.

### 6. Retry Logic for Bad Spartan-6 Bridge Replies

When a Spartan-6 bridge reply looked invalid, retry logic was added for:

- alternate TDO mask in XPCU control-bitbang mode
- alternate decode alignment for the bridge version and v2 SPI transfer path

### 7. Startup Sequence Experiments

The post-configuration startup path in `program_mem()` was changed more than once:

- initially modified to try Spartan-6-specific `ISC_DISABLE` handling
- later adjusted again because the short startup sequence likely prevented the bridge from becoming active

Current direction:

- keep the long post-`JSTART` clock run
- then do an extra Spartan-6 `ISC_DISABLE` pulse

## Observed Failure Modes by Stage

### Stage A: Initial SPI Access Failures

Early failures were simple flash-ID read failures:

- `Read ID failed`
- `SPI flash write failed`

This first suggested a flash DB problem, but that became less likely after confirming the expected flash type and that Xilinx tools worked.

### Stage B: Control-Bitbang Mode Made Things Worse

When XPCU control-transfer bitbang was forced:

- sometimes no JTAG devices were found
- sometimes USER scan traffic returned only `ff`

This suggested the control path itself was fragile on this cable / firmware combination.

### Stage C: Accelerated Path Returned All Zeroes

After returning to the accelerated path:

- chain detection still worked
- bridge version probe returned all zeros
- JEDEC read returned all zeros

This moved suspicion away from basic cable access and toward USER scan transport, bridge startup, or returned-bit alignment.

### Stage D: Internal Bridge Version Path Also Dead

This was the strongest clue:

- `SOJ version raw` came back all zero or all `ff`

Since `USER4` version access does not depend on talking to the flash correctly, this means the failure is not just "wrong SPI command sequence".

## Current Best Hypotheses

Ordered from strongest to weaker:

1. Spartan-6 bridge startup after SRAM configuration is still not complete enough for the user design to run.
2. XPCU accelerated USER scan readback is returning valid bits, but software is unpacking them with the wrong phase or byte alignment.
3. XPCU control-bitbang path samples `TDO` differently than needed for Spartan-6 USER scans, which explains the `ff`-heavy failures in that mode.
4. The bundled `spiOverJtag_xc6slx45tfgg484.bit.gz` bridge is valid, but the specific cable / firmware combination needs a more exact iMPACT-like startup or TAP-state sequence.

Less likely now:

- flash database mismatch
- wrong JEDEC constant for `W25Q64`
- missing quad enable as the root cause

Reason these are less likely:

- the internal bridge version path is already failing before useful SPI traffic begins

## Representative Logs

### Representative "all zero" accelerated-path failure

```text
SOJ version raw: 00 00 00 00 00 00 00
SOJ version decoded: 00 00 00 00 00 -> ''
SOJ version probe failed on Spartan-6; assuming bundled bridge uses v2 framing
SOJ version: 2.000000
...
Read ID failed: SPI RDID raw bytes: 00 00 00 00 -> 0x00000000
Fail
Read ID failed
```

### Representative "all ff" control-bitbang failure

```text
XPCU JTAG mode switched to control-transfer bitbang
...
SOJ version raw: ff ff ff ff ff ff 00
SOJ version decoded: ff ff ff ff fe -> '    ■'
...
Read ID failed: SPI RDID raw bytes: ff ff ff ff -> 0xffffffff
Fail
Read ID failed
```

### Representative "design loaded but bridge still dead" clue

```text
Shift IR 35
ir: 1 isc_done 1 isc_ena 0 init 1 done 1
```

Interpretation:

- configuration appears complete
- but USER bridge scans still do not return valid payloads

## What Was Learned

- The main blocker is no longer believed to be flash-chip support.
- The main blocker is no longer believed to be `.cor` import support.
- The problem sits near the boundary between:
  - Spartan-6 post-configuration startup
  - USER scan activation
  - XPCU USER scan transport / decode

## Current State Of The Tree

Files touched significantly during this work:

- [src/xilinx.cpp](/abs/path/C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/xilinx.cpp:1)
- [src/xilinxPlatformCableUSB.cpp](/abs/path/C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/xilinxPlatformCableUSB.cpp:1)
- [src/xilinxPlatformCableUSB.hpp](/abs/path/C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/xilinxPlatformCableUSB.hpp:1)
- [src/jtagInterface.hpp](/abs/path/C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/jtagInterface.hpp:1)
- [src/spiFlash.cpp](/abs/path/C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/src/spiFlash.cpp:1)

At the time this note was written, these files contained local uncommitted changes related to the investigation.

## Recommended Next Steps

1. Continue validating the revised Spartan-6 startup sequence and check whether `SOJ version raw` becomes non-zero.
2. If `SOJ version raw` is still dead, add raw accelerated XPCU RX tracing specifically around the `USER4` version probe and `USER1` flash reads.
3. Compare the exact TAP sequence used by iMPACT, if it can be observed, against `program_mem()` and the first bridge probe transaction.
4. If needed, add a Spartan-6-specific "post-load settle" experiment matrix:
   - more RTI clocks
   - `JSTART` only
   - `JSTART` then `ISC_DISABLE`
   - `ISC_DISABLE` then extra RTI clocks
   - optional return to `TLR` before first USER probe

## External References Used

- Xilinx / AMD Spartan-6 Configuration User Guide UG380
- AMD documentation for boundary-scan / USER register behavior

Useful links:

- https://docs.amd.com/v/u/en-US/ug380
- https://manualzz.com/doc/27686281/xilinx-spartan-6-fpga-user-guide

