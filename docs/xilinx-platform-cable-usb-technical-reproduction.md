# Xilinx Platform Cable USB: technical reproduction and debug record

Date: 2026-07-15  
Host used for the measurements: Windows, WinUSB, Docker Desktop  
Repository: `openFPGALoader`  
Target used for the successful test: Spartan-6 `XC6SLX45T-FGG484`

## 1. Scope and present result

This document reconstructs the complete debug procedure used to make Xilinx
Platform Cable USB devices usable from a native Windows openFPGALoader binary
built in Docker. It deliberately distinguishes two electrically similar but
firmware-incompatible cable families:

- the embedded/clone path, cold PID `03fd:000d`;
- genuine Platform Cable USB II/DLC10, cold PID `03fd:0013`.

The `000d` path is verified end to end. It loads two FX2 images, detects the
XC6SLX45T, and programs a 1.48 MB bitstream in approximately 4.1--4.6 seconds.

The `0013` path is verified through both FX2 stages. The first-stage loader was
recovered from the installed Xilinx kernel driver and the cable reaches FX2
version `0x0961`. On the tested cable, however, the internal programmable logic
then reports `0xFFFE`. Xilinx iMPACT normally replays `xusb_xp2.fmwr` through an
internal JTAG chain at this point. That last `.fmwr` interpreter is not yet
implemented in openFPGALoader, so a cold genuine DLC10 is not yet an end-to-end
standalone result. This is an explicit boundary of the current implementation,
not a target-board failure.

The state diagram is available as editable
[`diagrams/xpcu-firmware-state-machine.mmd`](diagrams/xpcu-firmware-state-machine.mmd)
and rendered
[`diagrams/xpcu-firmware-state-machine.svg`](diagrams/xpcu-firmware-state-machine.svg).

## 2. Hardware and software architecture

The cable has three distinct layers which must not be confused:

1. **Windows USB driver binding.** WinUSB only gives libusb access to a USB
   identity. It does not load cable firmware.
2. **Cypress FX2 firmware.** This RAM-resident 8051 program implements USB
   control requests and bulk endpoints. It disappears after complete power
   removal.
3. **Cable programmable logic.** Embedded cables expose a CPLD implementation;
   Platform Cable USB II contains a Spartan-3A-class internal FPGA/PROM path.
   It generates the fast JTAG waveforms for the external target connector.

The target FPGA is a fourth, independent JTAG TAP. A successful FX2 version
read proves only layers 1 and 2. `connected: yes` proves that target voltage is
sensed; neither result proves valid target TDO.

## 3. USB identities and firmware matrix

| VID:PID | openFPGALoader cable name | Meaning | Correct first image |
|---|---|---|---|
| `03fd:000d` | `xilinxPlatformCableUsb_alt` | Embedded cable cold loader | `xusb_emb.hex` |
| `03fd:0013` | `xilinxPlatformCableUsb` | Platform Cable USB II cold loader | `xusb_xp2_loader.hex` |
| `03fd:0008` | `xilinxPlatformCableUsb_initialized` | An FX2 image is running | Do not upload a boot image blindly |

The PID `0008` identity alone does not identify which operational firmware is
running. Read request `0x50` first.

| File | HEX version record | Role | Tested cable |
|---|---:|---|---|
| `xusb_emb.hex` | `0x0404` | initial FX2 image | cold PID `000d` |
| `xusb_xlp.hex` | `0x0517` | accelerated second-stage FX2 image | embedded/clone path |
| `xusb_xp2_loader.hex` | `0x08FD` (decimal 2301) | initial FX2 image extracted from `xusb_xp2.sys` | cold PID `0013` |
| `xusb_xp2.hex` | `0x0961` (decimal 2401) | operational second-stage FX2 image | genuine DLC10 |
| `xusb_xp2.fmwr` | proprietary microprogram | internal FPGA/PROM overlay/update | genuine DLC10 |

### Version byte order

The Intel HEX record at FX2 address `0x19B9` stores the printable four-digit
firmware value directly. For example:

```text
:0219B9000517...  -> 0x0517
:0219B9000961...  -> 0x0961
```

The FX2 `GET_VERSION` control response returns the low byte first, so host code
must decode `buf[1] << 8 | buf[0]`. Earlier debug output swapped the bytes and
called `0x0517` "1705". That display was wrong; it did not describe a different
firmware file.

## 4. Reproducible Windows driver deployment

Build the host-driver packages from the external project:

```powershell
Set-Location externals\xilinx-usb-driver
.\docker-build.ps1
```

Extract the Windows ZIP, start an elevated PowerShell in its Windows package,
and run:

```powershell
.\install.ps1
.\detect.ps1
```

`install.ps1` calls the Docker-built `wdi-simple.exe` once for each PID:
`0013`, `000d`, and `0008`. A success message for an absent PID can mean that
the package was staged, not that a live device changed driver. After firmware
causes PID `0008` to appear, rerun the elevated installer while `0008` is
physically present.

Verify with:

```powershell
Get-PnpDevice -PresentOnly | Where-Object InstanceId -Like 'USB\VID_03FD*'
```

Device Manager should list the live identity under **Universal Serial Bus
devices** using WinUSB. Xilinx iMPACT normally needs the vendor driver instead;
WinUSB intentionally replaces that binding.

## 5. Docker cross-build and firmware packaging

From the repository root:

```powershell
docker compose -f docker-compose.cross-windows.yml build windows-cross
docker compose -f docker-compose.cross-windows.yml run --rm windows-cross
```

The executable is installed at:

```text
dist/docker-windows/install/bin/openFPGALoader.exe
```

Firmware must be copied in `scripts/docker-cross-windows.sh`, while the
container is running. The Compose `/src` bind mount does not exist during a
Dockerfile `RUN` layer, so this does not work in `alpine.Dockerfile`:

```dockerfile
RUN cp /src/ise_programmer_bins/*.hex ...
```

The runtime script instead copies `ise_programmer_bins/*.hex` to:

```text
dist/docker-windows/install/share/openFPGALoader/
```

Always invoke the built executable by absolute path. Otherwise an older binary
on `PATH` can make a fixed source tree appear broken:

```powershell
$ofl = (Resolve-Path '.\dist\docker-windows\install\bin\openFPGALoader.exe').Path
& $ofl --scan-usb
```

## 6. Recovering the DLC10 loader from the installed Xilinx driver

ISE 14.7 was found at `E:\Xilinx\14.7`, not at a guessed root directory. The
relevant files are:

```text
E:\Xilinx\14.7\ISE_DS\common\bin\nt64\xusb_xp2.sys
E:\Xilinx\14.7\ISE_DS\ISE\data\xusb_xp2.hex
E:\Xilinx\14.7\ISE_DS\ISE\data\xusb_xp2.fmwr
E:\Xilinx\14.7\ISE_DS\ISE\data\xusbcpld.fmwr
```

Only the operational `0x0961` HEX is installed as a normal file. Loading it
directly into cold PID `0013` fails because the missing `2301` loader is
embedded in the Windows `.sys` driver.

Run the repository extractor:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass `
  -File .\scripts\extract-xusb-loader-from-sys.ps1 `
  -DriverPath 'E:\Xilinx\14.7\ISE_DS\common\bin\nt64\xusb_xp2.sys' `
  -OutputPath '.\ise_programmer_bins\xusb_xp2_loader.hex'
```

The explicit process-scoped execution-policy override is needed on hosts which
disable direct PowerShell script execution. It does not change the machine-wide
policy.

The extractor performs these steps:

1. validates the DOS `MZ` and PE signatures;
2. locates the PE `.data` section from the section table;
3. skips the first `0x100` bytes of `.data` globals;
4. reads records encoded as `uint16_le length`, `uint16_le address`, one zero
   type byte, `length` data bytes, and one zero padding byte;
5. rejects scanner artifacts whose FX2 address range overlaps an accepted
   record;
6. requires at least 500 records, an address-zero reset vector, a USB
   descriptor at `0x0090` containing `03fd:0008`, and a two-byte version record
   at `0x19B9`;
7. writes checksummed Intel HEX and an EOF record.

The tested driver yields 595 firmware records plus the HEX EOF line and version
`0x08FD`.

## 7. Cold-start procedures

### 7.1 Embedded cable, PID `000d` -- verified fast path

Fully remove USB and target power, reconnect, then run:

```powershell
& $ofl --scan-usb
& $ofl -c xilinxPlatformCableUsb_alt --detect -v
```

The expected state sequence is:

1. open `03fd:000d`;
2. upload `xusb_emb.hex` (`0x0404`) into FX2 RAM;
3. release FX2 reset and wait for `03fd:0008`;
4. reopen and reload `xusb_xlp.hex` (`0x0517`);
5. wait for `03fd:0008` again;
6. discover bulk endpoints `OUT=0x02`, `IN=0x86`;
7. select the external chain and prime accelerated transfer;
8. detect the target TAP.

The accelerator priming sequence reproduced from the reference driver is:

```text
disable -> speed class 0x11 -> enable -> A6 transfer count 2
-> bulk OUT 00 00 -> speed class 0x12 -> restore requested speed
```

The 24-bit A6 count is zero based (`number_of_bits - 1`).

Program SRAM with the same cable name and the desired `.bit` file:

```powershell
& $ofl -c xilinxPlatformCableUsb_alt --freq 6000000 .\path\design.bit -v
```

The verified XC6SLX45T IDCODE is `0x04028093` after the normal version-nibble
mask. The 1.48 MB test image completed in about 4.1--4.6 seconds.

### 7.2 Genuine Platform Cable USB II, PID `0013` -- current boundary

After a full power cycle:

```powershell
& $ofl --scan-usb
& $ofl -c xilinxPlatformCableUsb --detect -v
```

The expected implemented portion is:

```text
0013 -> xusb_xp2_loader.hex 0x08FD -> 0008
     -> xusb_xp2.hex 0x0961 -> 0008
```

The observed diagnostic after those successful reloads was:

```text
FX2 version:    0961
CPLD version:   fffe
Const0 version: 03b5
Const1 version: 0004
Const2 version: 200d
status 00 connected: no
TDO stuck at 0
```

`0xFFFE` is the decisive value: the cable-internal programmable logic has not
been configured. The external Spartan-6 cannot be reached through that state.
Do not debug the SP605 ribbon or its IDCODE until the internal overlay loads.

Binary strings in `libImpactComm.dll` show the vendor sequence uses
`playxsvf`, with `fpga`, `cpld`, and `cpld2` modes. The 734,080-byte
`xusb_xp2.fmwr` begins with version/comment metadata and a proprietary Xilinx
microprogram; it is not a plain FX2 HEX or a directly usable standard XSVF
file. USBPcap is installed at `C:\Program Files\USBPcap\USBPcapCMD.exe`; a
capture made while the official driver and iMPACT initialize a cold cable is
the next reliable way to recover the exact replay protocol.

### 7.3 Capturing the remaining official DLC10 initialization

This operation intentionally requires a manual driver transition. Do not run
the WinUSB and Xilinx stacks against the cable simultaneously.

1. Rebind `03fd:0013` and `03fd:0008` to the official ISE 14.7 Xilinx driver.
2. Remove power from the cable so the next session starts at PID `0013`.
3. In an elevated terminal, identify the USBPcap root-hub interface containing
   the cable, for example `\\.\USBPcap1`.
4. Start a full-length capture before opening iMPACT:

   ```powershell
   & 'C:\Program Files\USBPcap\USBPcapCMD.exe' `
     -d '\\.\USBPcap1' -A --inject-descriptors `
     -o '.\xpcu-xp2-official-init.pcap'
   ```

5. Open iMPACT and perform exactly one cable connect/detect. Wait until it can
   see the external chain, then stop USBPcap with Ctrl+C.
6. In Wireshark use display filter `usb.idVendor == 0x03fd`. Preserve the
   enumeration, control transfers, and both bulk endpoints; do not filter the
   capture at acquisition time.
7. Record the last `GET_VERSION` response before the `.fmwr` traffic, all
   `0x52` route-selection requests, every `0xA6` transfer length, bulk OUT
   payload, bulk IN verification data, delays, and the first version response
   after completion.
8. Close iMPACT, rebind all three PIDs to WinUSB with `install.ps1`, and fully
   power-cycle before testing a replay implementation.

The capture contains copyrighted Xilinx firmware bytes. Keep it as a local
reverse-engineering artifact and implement the transport/parser independently;
do not add the capture or extracted proprietary payload to a public commit.

## 8. XPCU USB protocol facts established during debugging

The operational device uses vendor request `bRequest=0xB0`. Important command
values are:

| `wValue` | Purpose |
|---:|---|
| `0x10` | disable |
| `0x18` | enable |
| `0x28` | set speed; class is carried in `wIndex` |
| `0x30` | synchronous GPIO write |
| `0x38` | status/TDO read |
| `0x40` | return fixed cable constants |
| `0x50` | return FX2/internal-logic version |
| `0x52` | select internal/external GPIO/JTAG routing |
| `0xA6` | prepare accelerated bulk JTAG transfer |

Endpoint discovery should select bulk OUT `0x02` and bulk IN `0x86` from the
active interface descriptors rather than assuming them without verification.

On Windows, a newly enumerated device can report configuration zero. The
libusb open path therefore reads the active configuration, sets configuration
1 when necessary, claims interface zero, and retries after
`LIBUSB_ERROR_NOT_FOUND`. Handles must be closed even when interface release
fails because firmware reload already disconnected the device.

## 9. Why the slow fallback produced misleading results

FX2 firmware `0x0404` stalls endpoint zero when the accelerated `0xA6` path is
used in the tested WinUSB and libusbK configurations. Continuing on the same
handle after that stall produced random-looking values such as `0x0C23EA80`
and `0xB8B1E8C0`. They were transport corruption, not FPGA IDCODEs. Perform a
complete power cycle after this failure.

The reliable fallback toggles TCK using synchronous control transfers: two USB
writes per JTAG bit, plus reads when TDO is captured. The printed 6 MHz value is
the requested cable clock, not effective host throughput. Reaching only 9% of
a bitstream after 5--10 minutes was therefore expected and implied a total
time approaching an hour.

The fallback also exposed two decoding details:

- command `0x38` returns the sample following the last shifted bit, so the
  `0x0404` TDO stream must be compensated by one bit;
- after a valid device, the tested cable returned trailing word `0x0A001093`
  instead of the usual all-ones terminator. It is ignored only as a trailing
  artifact after at least one valid TAP has been accepted.

Before correction, the one-bit-shifted value was `0xA2014049`. Restoring the
bit alignment yields the valid Spartan-6 family IDCODE `0x04028093` after
version masking.

## 10. Diagnostic environment variables

| Variable | Meaning |
|---|---|
| `OPENFPGALOADER_XUSB_FIRMWARE` | override the initial boot HEX |
| `OPENFPGALOADER_XUSB_XLP_FIRMWARE` | override embedded-cable second stage |
| `OPENFPGALOADER_XUSB_XP2_FIRMWARE` | override DLC10 second stage |
| `OPENFPGALOADER_XPCU_XLP_UPGRADE=1` | force XLP reload |
| `OPENFPGALOADER_XPCU_XP2_UPGRADE=1` | force XP2 reload |
| `OPENFPGALOADER_XPCU_SKIP_FIRMWARE_UPGRADE=1` | skip any second-stage reload |
| `OPENFPGALOADER_XPCU_SKIP_XLP_UPGRADE=1` | skip only XLP reload |
| `OPENFPGALOADER_XPCU_SKIP_XP2_UPGRADE=1` | skip only XP2 reload |
| `OPENFPGALOADER_XPCU_CONTROL_BITBANG=1` | force the slow control path |
| `OPENFPGALOADER_XPCU_ACCELERATED=1` | developer override for accelerated mode |
| `OPENFPGALOADER_FX2_VERBOSE_USB_ERRORS=1` | print detailed libusb failures |
| `OPENFPGALOADER_XPCU_CTRL_RETRIES` | control-request retry count |
| `OPENFPGALOADER_XPCU_CTRL_RETRY_DELAY_MS` | delay between control retries |
| `OPENFPGALOADER_XPCU_OUT_EP` / `..._IN_EP` | developer endpoint override |
| `OPENFPGALOADER_XPCU_TDO_MASK` | developer-only TDO mask override |

Unset experiments before a normal test:

```powershell
Get-ChildItem Env:OPENFPGALOADER_XPCU* | Remove-Item
Get-ChildItem Env:OPENFPGALOADER_XUSB* | Remove-Item
```

## 11. Failure signatures and decisions

| Symptom | Interpretation | Correct action |
|---|---|---|
| `--scan-usb` shows nothing | live PID lacks WinUSB or another process owns it | inspect Device Manager; rerun elevated installer for the live PID |
| remains `000d` or `0013` after complete power removal | normal cold state | select matching cable name and boot HEX |
| reload reaches 100% then returns to boot PID | wrong/incompatible HEX stage | restore the proper boot/upgrade pair |
| `LIBUSB_ERROR_NOT_FOUND` on claim | device has no active configuration | use build containing configuration-1 activation/retry |
| changing invalid IDCODE after A6 timeout | endpoint zero is poisoned | remove all cable/target power; do not trust that scan |
| stable `0x0A001093` after valid Spartan-6 word | XPCU control-mode trailing artifact | use corrected decoder/build |
| `0x0961`, internal version `0xFFFE`, status `00` | DLC10 internal FPGA overlay absent | replay `xusb_xp2.fmwr`; target JTAG is not yet reachable |
| `connected: yes`, no TAP | target voltage exists but JTAG data is invalid | then check power, ground, ribbon, jumpers, TDO and clock |

## 12. Integrity values for this exact reproduction

| File | Bytes | SHA-256 |
|---|---:|---|
| `xusb_emb.hex` | 21,708 | `C0A7A1F0110D110A2CE2A577EC1EBAF528FD066FEAC54F6FFC9816BE22FAC9D4` |
| `xusb_xlp.hex` | 26,216 | `03116AB70D74E68204E03F72AD8E45E3A22428CBE612B28DDEBCEC620954270A` |
| `xusb_xp2.hex` | 24,830 | `F551E3C71EFEE0556B465DC9975D528115F9849EA143F51B5C44D6F3F4F4A9CF` |
| `xusb_xp2_loader.hex` | 23,836 | `B6E3FAA81C34FAD863D2DDA261E62BBBCA6FAEA4BD6ECF2FF40572BFE71EC0FE` |
| ISE `xusb_xp2.sys` | 19,840 | `02A762D7BB32CCAA97460ED143AAC00286D961302D97B964C467654F42B1EE6A` |
| ISE `xusb_xp2.fmwr` | 734,080 | `91B15AAAA52A00E55B633F74D833D0C23061D021EEF4325592C5BA3126E6A75A` |
| ISE `xusbcpld.fmwr` | 90,076 | `FC1556D3BFE7486E3841F2E1B6DC62400D779ED853EB8D2662BC8943B3B0CFB4` |

The locally installed and repository `xusb_xp2.hex` hashes are identical.

## 13. Minimal regression checklist

For each cable family, start from a complete USB and target power removal.

```powershell
$ofl = (Resolve-Path '.\dist\docker-windows\install\bin\openFPGALoader.exe').Path
& $ofl --scan-usb
& $ofl -c xilinxPlatformCableUsb_alt --detect -v --freq 6000000
& $ofl -c xilinxPlatformCableUsb_initialized --detect -v --freq 6000000
```

Record all of these in a test log:

- PID before and after each reload;
- selected firmware path and parsed version;
- endpoint pair;
- FX2, internal logic, and constant versions;
- status byte and sensed connection state;
- requested and real JTAG frequency;
- whether control or accelerated transfer was selected;
- every IDCODE and process exit code;
- bitstream size, elapsed time, and DONE result.

Do not combine results taken before and after a failed `0xA6` request without a
power cycle. That was the main source of false conclusions in the original
session.

## 14. Related project documentation

- [Windows debug-session summary](xilinx-platform-cable-usb-windows-debug.md)
- [Driver deployment guide](../externals/xilinx-usb-driver/DEPLOYMENT.md)
- [Driver troubleshooting guide](../externals/xilinx-usb-driver/TROUBLESHOOTING.md)
- [DLC10 loader extractor](../scripts/extract-xusb-loader-from-sys.ps1)
- [AMD Platform Cable USB II Data Sheet, DS593](https://docs.amd.com/v/u/en-US/ds593)
- [AMD SVF and XSVF Formats, XAPP503](https://docs.amd.com/v/u/en-US/xapp503)
