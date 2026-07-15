# Xilinx Platform Cable USB II Windows debug session

Date: 2026-07-15
Host: Windows, WinUSB, Docker-built openFPGALoader  
Cable: Xilinx Platform Cable USB II (`VID 03fd`)

## Outcome

The Windows deployment and cable transport are working. openFPGALoader can:

1. find the cable in its boot identity;
2. upload `xusb_emb.hex`;
3. follow the USB disconnect/re-enumeration;
4. open the initialized cable;
5. read the FX2 and CPLD versions and cable status;
6. load the XLP `1705` second-stage firmware automatically;
7. use accelerated bulk JTAG transfers and program an XC6SLX45T successfully.

The final accelerated test programmed the 1.48 MB SP605
`led_blink_top.bit` image at 6 MHz in **4.124 seconds**. The earlier
control-transfer fallback had reached only 9% after 5--10 minutes.

The original `found 0 devices` result was traced to the control-transfer
fallback decoding XPCU status/TDO incorrectly. The official driver confirmed
that an XC6SLX45T was present. XPCU firmware `0404` exposes the captured stream
one bit late through command `0x38`; compensating that delay restored the
correct IDCODE. A second invalid word was the fallback's unreliable
end-of-chain response and is now treated as a trailing scan artifact only after
a valid device has already been found.

## USB identities

The cable changes identity while firmware is loaded. Both stages need a usable
Windows driver.

| Stage | USB ID | Meaning |
|---|---|---|
| Boot | `03fd:000d` or `03fd:0013` | FX2 bootloader, before firmware upload |
| Initialized | `03fd:0008` | Firmware is running and the cable is ready |

The firmware is volatile. After a complete power removal, seeing the boot
identity again is expected.

## Initial symptoms

- The installer completed, but Device Manager showed **Xilinx Platform Cable
  USB embedded (bootloader)** and `openFPGALoader --scan-usb` did not initially
  show a usable cable.
- Firmware upload later succeeded, but the accelerated JTAG request timed out.
- Falling back after that timeout also failed and returned changing, invalid
  IDCODEs such as `0x0c23ea80` and `0xb8b1e8c0`.

Those invalid values were not FPGA IDCODEs. The accelerated request had stalled
USB endpoint zero, so the subsequent fallback was operating on a poisoned
connection.

## Findings and fixes

### 1. Bind WinUSB to both enumeration stages

The installation package covers `03fd:0013`, `03fd:000d`, and `03fd:0008`.
The initialized `0008` device was initially still associated with an older
libusbK installation. Running `install.ps1` again while `0008` was physically
present installed WinUSB for that live identity.

The installer now warns when an identity was only staged because the device was
not connected. In that case, upload the firmware, wait for `0008`, and rerun the
installer as Administrator.

### 2. Recognize an already initialized cable

The cable registry now includes `03fd:0008` as
`xilinxPlatformCableUsb_initialized`. When this identity is selected,
openFPGALoader opens it directly instead of attempting another firmware upload.

Relevant source:

- [`src/cable.hpp`](../src/cable.hpp)
- [`src/xilinxPlatformCableUSB.cpp`](../src/xilinxPlatformCableUSB.cpp)

### 3. Select the external JTAG chain

The initialization sequence sends XPCU command `0x52`, selecting the external
JTAG connector before transfers begin.

### 4. Match the transfer method to the running firmware

With EMB firmware `0404`, accelerated XPCU request `0xA6` times out and stalls
endpoint zero. That firmware therefore uses the reliable but slow
control-transfer implementation. XLP firmware `1705` supports the accelerated
engine after it is primed with the reference speed/transfer sequence; current
builds select this path automatically.

Further tests ruled out the Windows backend and interface selection as the
cause. The same `0xA6` request timed out with both WinUSB and libusbK, and both
with normal alternate-setting selection and with selection bypassed. Each
timeout left endpoint zero unusable until the cable was fully power-cycled.

The reverse-engineered protocol defines the 24-bit A6 transfer count as
zero-based (`N - 1`). An experimental actual-count (`N`) build timed out in the
same way and was reverted.

### 5. Firmware variants and upload speed

The working `xusb_emb.hex` reports firmware `0404`. Its control-transfer path
requires two synchronous GPIO writes for every JTAG bit, plus status reads when
TDO is captured. The displayed JTAG frequency is therefore the cable clock
setting, not the achieved host-to-cable throughput. A bitstream upload reaching
only 9% after 5--10 minutes is expected with this fallback and can take roughly
an hour.

The newly supplied `xusb_xlp.hex` is genuine XLP firmware `1705`, but it is an
upgrade image rather than standalone boot firmware. Copying it over
`xusb_emb.hex` made the cable fall back to boot PID `03fd:000d` after the reload
wait. The correct pair is:

- `xusb_emb.hex` version `0404` for initial boot and enumeration as `0008`;
- `xusb_xlp.hex` version `1705` for the second-stage reload and accelerated JTAG.

The loader reads the embedded HEX version, automatically performs this
two-stage sequence for cold PID `000d`, reopens PID `0008`, and primes the
accelerator. `xusb_xp2.hex` remains incompatible with this boot identity.

### 6. Close the USB handle after disconnects or release errors

The FX2 cleanup path now closes the libusb handle even if interface release
fails because the device has disconnected. This was verified by running two
detections against initialized PID `0008` without unplugging between them.

Relevant source:

- [`src/fx2_ll.cpp`](../src/fx2_ll.cpp)

## Verification results

| Test | Result |
|---|---|
| Scan boot PID `03fd:000d` | Detected as `xilinxPlatformCableUsb_alt` |
| Upload `xusb_emb.hex` | Completed successfully |
| Re-enumerate as PID `03fd:0008` | Completed successfully |
| Discover endpoints | `OUT=0x02`, `IN=0x86` |
| Read cable versions | FX2 `0404`, CPLD `1200` |
| Read status | `0x43`, `connected: yes` |
| EMB `0404` accelerated `0xA6` path | Timed out; use its control fallback |
| Accelerated path with libusbK | Same timeout as WinUSB; backend ruled out |
| Accelerated path without alternate-setting selection | Same timeout; setting ruled out |
| Experimental A6 count `N` instead of `N-1` | Same timeout; reverted to documented zero-based count |
| `xusb_xp2.hex` on boot PID `000d` | Incompatible versions/status; do not use |
| Forced control path after power cycle | Stable, no USB timeout, exit code 0 |
| Second initialized-PID run without reconnect | Stable, proving teardown/reopen works |
| Target TAP detection, old TDO mask `0x02` | `found 0 devices` |
| Old delayed TDO stream with mask `0x01` | One-bit-shifted IDCODE `0xa2014049` |
| Corrected control decoder | `0x04028093`, `xc6slx45T`, IR length 6 |
| Repeated detection at 750 kHz and 6 MHz | Success, exit code 0 |
| EMB `0404` then XLP `1705` automatic reload | Success; PID `0008` reopened |
| XLP `1705` accelerated detect at 6 MHz | XC6SLX45T found, exit code 0 |
| XLP `1705` accelerated SRAM program | 1.48 MB bitstream in 4.124 seconds, DONE=1 |
| Digilent `0403:6014` (`digilent_hs2`) | Works at 6 MHz, but reaches a different XC7A35T board |

The official driver identified the panel target as an XC6SLX45T. The diagnostic
value was not random: shifting `0xa2014049` left by one and restoring mandatory
IDCODE bit 0 gives `0x44028093`. Ignoring the version nibble, this matches the
project database entry `0x04028093`. Raw tracing confirmed delayed samples
`0x22014049`; the corrected decoder reconstructs `0x44028093` before the normal
version mask is applied.

The next scan word was consistently `0x0a001093` instead of the all-ones chain
terminator. Because the first device was already valid and this behavior is
specific to XPCU control mode, detection now warns and stops at that trailing
artifact. It reports one device:

```text
found 1 devices
index 0:
    idcode 0x4028093
    manufacturer xilinx
    family spartan6
    model  xc6slx45T
    irlength 6
```

## Known-good commands

Run the executable by its full path so an older copy from `PATH` is not tested
accidentally:

```powershell
$ofl = 'C:\Users\livanyi\Desktop\WORK\GIT\openFPGALoader\dist\docker-windows\install\bin\openFPGALoader.exe'
& $ofl --scan-usb
& $ofl -c xilinxPlatformCableUsb_alt --detect -v
```

To force the safe path explicitly:

```powershell
$env:OPENFPGALOADER_XPCU_CONTROL_BITBANG = '1'
& $ofl -c xilinxPlatformCableUsb_alt --detect -v --freq 750000
Remove-Item Env:OPENFPGALOADER_XPCU_CONTROL_BITBANG
```

After firmware is already loaded, scanning may report:

```text
03fd:0008 xilinxPlatformCableUsb_initialized XILINX none XILINX
```

That is a healthy initialized state, not an error.

## Target-side checks when a corrected build still finds no TAP

1. Completely remove power from both cable and target, then reconnect them.
2. Confirm the target board is powered and has a common ground with the cable.
3. Verify the ribbon cable orientation and the exact Xilinx JTAG header pinout.
4. Check JTAG-enable or boot-mode jumpers and remove any competing programmer.
5. Reduce JTAG frequency and retry, for example `--freq 100000`.
6. If possible, verify the same board and ribbon with a known-good programmer.

## Environment switches used during diagnosis

| Variable | Purpose |
|---|---|
| `OPENFPGALOADER_XUSB_FIRMWARE` | Select a firmware HEX file explicitly |
| `OPENFPGALOADER_XPCU_CONTROL_BITBANG=1` | Force control-transfer JTAG immediately |
| `OPENFPGALOADER_XUSB_XLP_FIRMWARE` | Select the XLP second-stage HEX explicitly |
| `OPENFPGALOADER_XPCU_XLP_UPGRADE=1` | Force the XLP second-stage reload |
| `OPENFPGALOADER_XPCU_SKIP_XLP_UPGRADE=1` | Disable the automatic cold-boot XLP reload |
| `OPENFPGALOADER_FX2_VERBOSE_USB_ERRORS=1` | Print additional libusb diagnostics |
| `OPENFPGALOADER_XPCU_TDO_MASK=0x01` or `0x02` | Diagnostic TDO-bit selection; do not use as a normal fix |

## References

- [XPCU reverse-engineering notes](https://diamondman.github.io/Adapt/cable_xilinx_PCU.html)
- [Driver deployment guide](../externals/xilinx-usb-driver/DEPLOYMENT.md)
- [Practical troubleshooting guide](../externals/xilinx-usb-driver/TROUBLESHOOTING.md)
