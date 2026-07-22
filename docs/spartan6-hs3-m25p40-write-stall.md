# Spartan-6 HS3 M25P40 Write Stall

## Symptom

This command reached the external SPI flash, detected a valid JEDEC response, and then appeared to stop:

```powershell
.\openFPGALoader.exe `
  -c digilent_hs3 `
  --fpga-part xc6slx9tqg144 `
  --external-flash `
  -f ..\..\..\..\test_fw\EL-24-02\mcc_servo_fpga_fw-0v3-2024_0620_0922.mcs
```

Observed log:

```text
write to flash
Jtag frequency : requested 6.00MHz    -> real 6.00MHz
Open file ...mcc_servo_fpga_fw-0v3-2024_0620_0922.mcs DONE
Parse file DONE
Open file ...\spiOverJtag_xc6slx9tqg144.bit.gz DONE
Parse file DONE
Use: ...\spiOverJtag_xc6slx9tqg144.bit.gz
load program
Load SRAM: [==================================================] 100.00%

Done
Shift IR 35
ir: 1 isc_done 1 isc_ena 0 init 1 done 1
SOJ version: 1.000000
Detail:
Jedec ID          : 20
memory type       : 20
memory capacity   : 13
```

This was different from the earlier Xilinx Platform Cable USB failure. Here the Digilent HS3 cable worked, JTAG worked, the Spartan-6 bridge loaded, and the SPI flash answered `RDID`.

## What The Fields Mean

`Shift IR 35` means the Spartan-6 `USER1` instruction was selected. For Spartan-6, `USER1` is opcode `0x02`, but the JTAG stream is shifted LSB-first through a 6-bit IR; the displayed host byte can look different after packing/bit ordering.

`ir: 1 isc_done 1 isc_ena 0 init 1 done 1` means the FPGA configuration status looked healthy after loading the temporary SPI-over-JTAG bridge:

- `done 1`: FPGA configured.
- `init 1`: configuration init line is healthy.
- `isc_done 1`: internal configuration operation completed.
- `isc_ena 0`: ISC programming mode is not still enabled.

`SOJ version: 1.000000` means the bridge version probe did not decode a v2 string, so openFPGALoader used the v1 SPI-over-JTAG framing. This is normal for the packaged `spiOverJtag_xc6slx9tqg144.bit.gz` path that successfully read the flash ID.

`Jedec ID 20, memory type 20, memory capacity 13` is the SPI flash response to command `0x9F`. Combined, that is JEDEC ID:

```text
0x202013
```

That identifies an ST/Micron M25P40-class SPI flash, a small 4 Mbit / 512 KiB device.

## Root Cause

The flash ID `0x202013` was not present in `src/spiFlashdb.hpp`.

Because the ID was unknown, `SPIFlash::read_id()` printed only the raw detail fields:

```text
Detail:
Jedec ID          : 20
memory type       : 20
memory capacity   : 13
```

It did not set `_flash_model`.

That left the later write path without a proper flash model:

- no known flash size
- no known sector count
- no known erase capabilities
- no known block-protection layout
- no user-friendly `Detected: ...` message

The code could then continue toward status/protection/erase/write using only generic fallback behavior. On this Spartan-6/HS3 path it looked like the operation was stuck immediately after flash ID output.

## Why The MCS File Fits

The MCS file itself is about 938 KiB on disk because Intel HEX/MCS is ASCII text with address records and line overhead. That does not mean the binary payload is 938 KiB.

The addressed payload range was checked:

```text
Min address:     0x0
Max exclusive:   0x53605
Payload bytes:   341165
Range bytes:     341509
Flash size:      0x80000 / 524288 bytes
Fits M25P40:     yes
```

So the firmware image fits inside a 512 KiB M25P40-class flash.

## Fix

Added `0x202013` to the SPI flash database as an ST/Micron M25P40-class device:

```cpp
{0x202013, {
    .manufacturer = "ST",
    .model = "M25P40",
    .nr_sector = 8,
    .sector_erase = true,
    .subsector_erase = false,
    .has_extended = false,
    .tb_otp = true,
    .tb_offset = 0,
    .tb_register = STATR,
    .bp_len = 3,
    .bp_offset = {(1 << 2), (1 << 3), (1 << 4), 0},
    .quad_register = NONER,
    .quad_mask = 0,
    .global_lock = false,
}},
```

File:

```text
src/spiFlashdb.hpp
```

Also extended the existing ST M25P protection handling so M25P40 is treated like the M25P family parts that have no top/bottom protection bit:

```cpp
if (((jedec == 0x202013) || (jedec == 0x202015) ||
    (jedec == 0xC22017))
    && tb == 1 && base_addr != 0) {
    _unprotect = true;
    _must_relock = true;
}
```

File:

```text
src/spiFlash.cpp
```

Added a diagnostic boundary in the write path:

```text
Read flash status: DONE
```

File:

```text
src/flashInterface.cpp
```

This makes future logs clearer. If a board still stalls, the log will show whether it stopped before status read, during erase, or during page programming.

## Expected Log After The Fix

After rebuilding the Windows dist executable, the same command should no longer stop after raw JEDEC detail. It should identify the flash model:

```text
JEDEC ID: 0x202013
Detected: ST M25P40 8 sectors size: 4Mb
Read flash status: DONE
Erase Flash:
...
Writing:
...
```

## Practical Notes

This issue was not a Spartan-6 USER opcode problem. The same run already proved that:

- Digilent HS3 JTAG transport worked.
- Spartan-6 chain detection worked.
- The temporary `spiOverJtag_xc6slx9tqg144.bit.gz` bridge loaded.
- USER/SPI access worked enough to read JEDEC ID.

The missing piece was flash database support for the detected small SPI flash.

