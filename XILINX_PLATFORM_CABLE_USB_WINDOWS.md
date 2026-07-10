# Xilinx Platform Cable USB on Windows

This note covers the `xilinxPlatformCableUsb_alt` backend on portable Windows
builds.

## Expected USB states

Before firmware load, some SP605/Xilinx embedded Platform Cable devices appear
as:

```text
03fd:000d
```

After `xusb_emb.hex` is loaded, the same device can re-enumerate as:

```text
03fd:0008
```

Both states must be accessible to libusb. On Windows, use Zadig and bind the
Xilinx device/interface to WinUSB or libusbK. If only the boot state is bound,
firmware upload can succeed but JTAG transfers can fail after reload.

The public `xilinx-xusb` mirror documents udev entries for Xilinx PIDs such as
`0007`, `0008`, `0009`, `000d`, `000f`, `0013`, and `0015`.

## Runtime firmware lookup

The portable package should contain:

```text
install/share/openFPGALoader/xusb_emb.hex
```

Runtime lookup order is:

```text
1. --probe-firmware <path>
2. OPENFPGALOADER_XUSB_FIRMWARE
3. share/openFPGALoader/xusb_emb.hex beside the portable install
4. legacy Vivado/ISE paths
```

## Endpoint and alternate-setting debug

Some Windows/libusbK installations expose the post-firmware interface but reject
`libusb_set_interface_alt_setting(0, 1)` with `LIBUSB_ERROR_IO`. If control
transfers work but JTAG bulk transfers fail with:

```text
FX2 write error: LIBUSB_ERROR_NOT_FOUND
JTAG init failed with: TDO is stuck at 0
```

then the bulk endpoint pair is not available in the currently selected alternate
setting.

This fork scans the USB descriptors and tries to select the alternate setting
that contains one bulk OUT endpoint and one bulk IN endpoint. It still defaults
to the classic Xilinx endpoints:

```text
OUT 0x02
IN  0x86
```

For manual testing, these environment variables are available:

```powershell
$env:OPENFPGALOADER_XPCU_OUT_EP = "0x02"
$env:OPENFPGALOADER_XPCU_IN_EP  = "0x86"
```

To skip alternate-setting selection entirely:

```powershell
$env:OPENFPGALOADER_XPCU_SKIP_ALT_SETTING = "1"
```

Clear them again with:

```powershell
Remove-Item Env:\OPENFPGALOADER_XPCU_OUT_EP -ErrorAction SilentlyContinue
Remove-Item Env:\OPENFPGALOADER_XPCU_IN_EP -ErrorAction SilentlyContinue
Remove-Item Env:\OPENFPGALOADER_XPCU_SKIP_ALT_SETTING -ErrorAction SilentlyContinue
```

Normal test command:

```powershell
.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --freq 700000 --detect
```


## Control-transfer timeout after reload

Some SP605 / Xilinx Platform Cable USB combinations enumerate correctly after
FX2 firmware load, and descriptor endpoint discovery can succeed, but the first
vendor control read may still time out:

```text
XPCU bulk endpoints: OUT=0x02 IN=0x86
Unable to read control request: LIBUSB_ERROR_TIMEOUT
JTAG init failed with: Unable to read constant.
```

The Windows backend now retries early XPCU control transfers and uses a longer
FX2 control timeout by default.

Useful debug overrides:

```powershell
$env:OPENFPGALOADER_FX2_CTRL_TIMEOUT_MS = "2000"
$env:OPENFPGALOADER_XPCU_CTRL_RETRIES = "60"
$env:OPENFPGALOADER_XPCU_CTRL_RETRY_DELAY_MS = "100"
.\openFPGALoader.exe -c xilinxPlatformCableUsb_alt --freq 700000 --detect
```

Use normal `.\openFPGALoader.exe` in the command above; the escaped marker is
only to avoid accidental formatting in some markdown renderers.
