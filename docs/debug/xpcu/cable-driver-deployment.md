# XPCU cable driver deployment

This page documents the host USB driver packages built from the
[`xsession/xilinx-usb-driver`](https://github.com/xsession/xilinx-usb-driver)
submodule (`externals/xilinx-usb-driver`), which openFPGALoader's Windows
release embeds. It covers vendor ID `03fd` and product IDs
`0013`/`000d` (before firmware loading) and `0008` (after the cable
re-enumerates).

> **Note**
> The canonical source of truth lives in the submodule
> (`externals/xilinx-usb-driver/DEPLOYMENT.md`). This copy is kept with the
> other [XPCU field notes](technical-reproduction.md) so the documentation
> site is self-contained.

## Build all packages with Docker

From `externals/xilinx-usb-driver`, run `sh ./docker-build.sh`. On Windows
PowerShell, run `.\docker-build.ps1`.

The build creates `dist/xilinx-platform-cable-{linux,windows,macos}.zip` and
SHA-256 files. Docker builds the Linux i686/x86_64 iMPACT compatibility
libraries and the x86/x64 Windows libwdi helper. Host installation is kept
out of containers because containers cannot change Windows/macOS host
drivers or Linux host udev policy.

## Linux

Extract the Linux ZIP and run `sudo ./install.sh`. Reconnect the cable, then:

```sh
openFPGALoader --cable xilinxPlatformCableUsb --detect
```

The libraries under `lib/` are only for legacy Xilinx ISE/iMPACT. Select the
directory matching the ISE process architecture and preload it, for example:

```sh
LD_PRELOAD="$PWD/lib/x86_64/libusb-driver.so" impact
```

Firmware (`xusb_xp2.hex` or `xusb_emb.hex`) remains Xilinx software and is
not redistributed. Give openFPGALoader the file with `--probe-firmware`, set
`OPENFPGALOADER_XUSB_FIRMWARE`, or use firmware installed with ISE/Vivado.

## Windows

Extract the Windows ZIP, open PowerShell as Administrator, and run:

```powershell
.\install.ps1
.\detect.ps1
```

The installer binds WinUSB to all three XPCU identities using the
Docker-built libwdi helper. This intentionally replaces the vendor driver
for these device IDs. Reinstall the AMD/Xilinx cable driver from Vivado if
vendor applications later require it.

The Windows build uses the reliable USB control-transfer JTAG path by
default. The accelerated `0xA6` path can be tested explicitly by setting
`OPENFPGALOADER_XPCU_ACCELERATED=1`; some XPCU firmware/WinUSB combinations
stall endpoint zero when that command is used.

For bootloader, driver-binding, firmware-reload, and JTAG-chain symptoms,
see [cable driver troubleshooting](cable-driver-troubleshooting.md). The
complete hardware debug record is in the
[Windows debug report](windows-debug-report.md).

## macOS

Extract the macOS ZIP and run `./install.sh`. macOS needs libusb and a
libusb-enabled openFPGALoader build; it does not use a separately installed
XPCU kernel driver. The helper detects the cable and installs Homebrew
libusb when Homebrew is available.

## Container USB limitation

The produced files are intended for native host use. Linux can pass the
cable to a container with an explicit `/dev/bus/usb` mapping, but the cable
disconnects and re-enumerates while firmware loads, so mapping the entire
USB bus is usually required. Docker Desktop on Windows and macOS does not
provide equivalent transparent USB passthrough; run openFPGALoader on those
hosts.
