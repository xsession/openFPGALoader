# XPCU cable driver troubleshooting

Practical fixes for Xilinx Platform Cable USB / USB II use with the
deployment packages from the
[`xsession/xilinx-usb-driver`](https://github.com/xsession/xilinx-usb-driver)
submodule, especially the Windows WinUSB package.

> **Note**
> The canonical source of truth lives in the submodule
> (`externals/xilinx-usb-driver/TROUBLESHOOTING.md`). This copy is kept with
> the other [XPCU field notes](technical-reproduction.md) so the
> documentation site is self-contained.

## First check

Use the exact openFPGALoader executable you just built or installed. An older
copy found through `PATH` may not include the XPCU fixes.

```powershell
$ofl = 'C:\path\to\openFPGALoader.exe'
& $ofl --scan-usb
& $ofl -c xilinxPlatformCableUsb_alt --detect -v
```

The cable normally appears as one of these identities:

| USB ID | State | Probe name |
|---|---|---|
| `03fd:0013` | Bootloader variant | `xilinxPlatformCableUsb` |
| `03fd:000d` | Embedded bootloader | `xilinxPlatformCableUsb_alt` |
| `03fd:0008` | Firmware initialized | `xilinxPlatformCableUsb_initialized` |

Firmware is held in RAM. Returning to `000d` or `0013` after all power is
removed is normal.

## Windows: cable is absent from `--scan-usb`

1. Open PowerShell as Administrator in the extracted Windows driver package.
2. Run `./install.ps1`, unplug/reconnect the cable, then run `./detect.ps1`.
3. Check Device Manager under **Universal Serial Bus devices**.
4. If firmware loading changes the PID to `0008`, rerun `install.ps1` while
   that `0008` device is present. A staged package does not always replace
   an existing libusbK/vendor binding until the matching live device is
   updated.

The package intentionally replaces the AMD/Xilinx driver with WinUSB for
these IDs. Reinstall the Vivado cable driver later if Xilinx vendor tools
require it.

## Cable stays at the bootloader identity

The driver alone does not permanently initialize the cable. For embedded
boot PID `03fd:000d`, openFPGALoader first uploads `xusb_emb.hex` version
`0404`. When `xusb_xlp.hex` version `0517` is installed, current builds then
load it as a second-stage upgrade and enable accelerated transfers
automatically.

Make the firmware available with one of these methods:

```powershell
$env:OPENFPGALOADER_XUSB_FIRMWARE = 'C:\path\to\xusb_emb.hex'
& $ofl -c xilinxPlatformCableUsb_alt --detect -v
```

Alternatively use `--probe-firmware` or install both files in the normal
openFPGALoader data directory.

Expected progress is firmware upload followed by a USB reload and PID
`0008`.

## Device Manager reports Code 10

Remove power completely from the cable and target, wait a few seconds, and
reconnect. Then verify the driver again. A normal unplug may not reset every
state involved after a failed USB control request.

## Firmware reload fails after adding a newer HEX file

Do not replace `xusb_emb.hex` with `xusb_xlp.hex`. XLP `0517` is a
second-stage upgrade and is not a standalone boot image for PID `03fd:000d`.
The required pair is EMB `0404` as `xusb_emb.hex` and XLP `0517` as
`xusb_xlp.hex`.

## Accelerated transfer times out or produces random IDCODEs

On EMB firmware `0404`, accelerated XPCU command `0xA6` stalls endpoint
zero. Any IDCODE read after that timeout can be meaningless. Current builds
keep `0404` on the control-transfer fallback, but automatically use the
accelerated engine when XLP firmware `0517` is running.

Force the safe mode before opening the cable if needed:

```powershell
$env:OPENFPGALOADER_XPCU_CONTROL_BITBANG = '1'
& $ofl -c xilinxPlatformCableUsb_alt --detect -v --freq 750000
```

Testing the initialized PID with libusbK instead of WinUSB produced the same
`0404` timeout, so changing the Windows backend alone does not make that
firmware accelerated. Bypassing USB alternate-setting selection also
produced the same result.

The reliable control fallback is much slower than the frequency message
suggests: it uses two synchronous USB control writes per JTAG bit and
additional reads for captured TDO. Large Spartan-6 uploads can consequently
take close to an hour.

Do not change the XLP `0517` A6 request from its tested zero-based value
`N-1` to `N`. The latter makes common 32-bit scans a multiple-of-four XPCU
CPLD failure case, stalls bulk IN, and requires a full power cycle.

Do not substitute `xusb_xp2.hex` for an embedded-loader (`03fd:000d`)
cable. On the tested unit it reported invalid versions and no connection.

With the proper `0404`/`0517` pair, the tested 1.48 MB Spartan-6 bitstream
programmed through WinUSB at 6 MHz in 4.124 seconds.

## Firmware and cable status work, but detection says `found 0 devices`

If the log shows all of the following, the host USB driver is operating:

```text
XPCU bulk endpoints: OUT=0x02 IN=0x86
FX2 version:    0404
CPLD version:   1200
status 43 connected: yes
```

Current builds use XPCU status bit 0 and compensate for firmware `0404`
returning control-mode TDO one bit late. Older builds could report no
devices, an IDCODE shifted right by one, or a correctly framed but invalid
Xilinx word, even though the official driver detected the target. Rebuild
openFPGALoader before treating this as a hardware fault.

With the corrected build, a single-device chain can emit a warning such as:

```text
ignoring XPCU trailing scan word 0x0a001093
```

This is the unreliable end-of-chain marker from the slow fallback. It is
ignored only after a valid device has already been decoded.

If a current build still reports `found 0 devices`, check:

- target-board power and common ground;
- ribbon orientation and header pinout;
- JTAG-enable/boot-mode jumpers;
- another programmer or circuit driving the same signals;
- cable/adapter continuity;
- a slower clock, such as `--freq 100000`.

`connected: yes` indicates sensed target voltage; it does not prove that a
TAP is shifting valid TDO data.

## Interface claim fails on a second run

Close other programming applications and retry. Current openFPGALoader
cleanup closes the USB handle even if the device disconnected during
interface release. If an older binary still holds the interface, terminate
it or power-cycle the cable, then test using the newly built executable's
full path.

## Diagnostic environment variables

| Variable | Effect |
|---|---|
| `OPENFPGALOADER_XUSB_FIRMWARE` | Explicit firmware HEX path |
| `OPENFPGALOADER_XPCU_CONTROL_BITBANG=1` | Force safe control-transfer mode |
| `OPENFPGALOADER_XUSB_XLP_FIRMWARE` | Explicit XLP second-stage HEX path |
| `OPENFPGALOADER_XPCU_XLP_UPGRADE=1` | Force the XLP second-stage reload |
| `OPENFPGALOADER_XPCU_SKIP_XLP_UPGRADE=1` | Disable automatic cold-boot XLP reload |
| `OPENFPGALOADER_FX2_VERBOSE_USB_ERRORS=1` | Enable detailed libusb errors |
| `OPENFPGALOADER_XPCU_TDO_MASK` | Developer-only TDO mask diagnostic (`0x01` or `0x02`) |

For the full measurements, code changes, and test matrix, see the
[Windows debug report](windows-debug-report.md).
