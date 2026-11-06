 # Digilent HS3 vs Xilinx Platform Cable USB Dump-Flash Codepath

This note compares these two commands:

```powershell
.\openFPGALoader.exe `
  -c digilent_hs3 `
  --fpga-part xc6slx9tqg144 `
  --external-flash `
  --dump-flash `
  --file-size 16777216 `
  -o 0 `
  flash_dump.bin `
  -v
```

```powershell
.\openFPGALoader.exe `
  -c xilinxPlatformCableUsb `
  --fpga-part xc6slx9tqg144 `
  --external-flash `
  --dump-flash `
  --file-size 16777216 `
  -o 0 `
  flash_dump.bin `
  -v
```

## Short conclusion

The two commands are identical from the FPGA/flash point of view once JTAG is initialized. Both request a Spartan-6 external SPI flash dump through the Xilinx `dumpFlash()` path.

The difference is entirely caused by `-c`:

- `digilent_hs3` selects the FTDI MPSSE backend. In the successful log it reaches JTAG chain detection, loads `spiOverJtag_xc6slx9tqg144.bit.gz`, reads the SPI flash JEDEC ID, and dumps flash.
- `xilinxPlatformCableUsb` selects the Xilinx Platform Cable USB / FX2 backend. In the failing log it loads `xusb_xp2_loader.hex`, waits for USB re-enumeration, then cannot reopen the post-firmware FX2 device. It fails before JTAG chain detection, before bridge loading, before JEDEC ID read, and before any flash dump.

So the observed failure for `-c xilinxPlatformCableUsb` is not a Spartan-6 USER opcode problem and not a SPI flash problem. It is a cable initialization/re-enumeration problem in the XPCU backend.

## Cable definitions

The command-line cable name is looked up in `cable_list` in `src/cable.hpp`.

Relevant entries:

```cpp
{"digilent_hs3",       FTDI_SER(0x0403, 0x6014, FTDI_INTF_A, 0x88, 0x8B, 0x20, 0x30)},
{"xilinxPlatformCableUsb",     CABLE_DEF(MODE_XPCU, 0x03fd, 0x0013                 )},
{"xilinxPlatformCableUsb_alt", CABLE_DEF(MODE_XPCU, 0x03fd, 0x000D                 )},
{"xilinxPlatformCableUsb_initialized", CABLE_DEF(MODE_XPCU, 0x03fd, 0x0008         )},
```

Source: `src/cable.hpp:105`, `src/cable.hpp:142-144`.

Practical meaning:

- `digilent_hs3` is `MODE_FTDI_SERIAL`, VID:PID `0403:6014`, FTDI interface A, with Digilent-specific initial GPIO values/directions.
- `xilinxPlatformCableUsb` is `MODE_XPCU`, VID:PID `03fd:0013`, the uninitialized/loader identity used by one Xilinx Platform Cable USB variant.
- `xilinxPlatformCableUsb_alt` is also `MODE_XPCU`, but starts from VID:PID `03fd:000d`.
- The initialized XPCU identity is `03fd:0008`.

## CLI parsing and common request fields

`--dump-flash` is parsed into `Device::RD_FLASH`:

```cpp
else if (result.count("dump-flash"))
    args->prg_type = Device::RD_FLASH;
```

Source: `src/main.cpp:1191-1204`.

The dump is executed later by:

```cpp
if (args.prg_type == Device::RD_FLASH) {
    if (args.file_size == 0) {
        printError("Error: 0 size for dump");
    } else {
        fpga->dumpFlash(args.offset, args.file_size);
    }
}
```

Source: `src/main.cpp:735-740`.

Practical meaning:

- `--dump-flash` chooses read-flash mode.
- `--file-size 16777216` becomes the number of bytes passed to `dumpFlash()`.
- `-o 0` becomes the `base_addr` argument.
- `flash_dump.bin` is the output file name stored in `args.bit_file`.
- `--external-flash` makes the Xilinx object use SPI flash access mode through a bridge.

These fields are the same in both commands. They only matter after JTAG initialization succeeds.

## Where the paths split

After cable lookup, `main()` constructs a `Jtag` object:

```cpp
jtag = new Jtag(cable, &pins_config, args.device, args.usb_serial_num,
        args.freq, args.verbose, args.ip_adr, args.port,
        args.invert_read_edge, args.probe_firmware,
        args.user_misc_devs);
```

Source: `src/main.cpp:459-464`.

Inside `Jtag::Jtag()`, the backend is selected by `cable.type`:

```cpp
case MODE_FTDI_SERIAL:
    _jtag = new FtdiJtagMPSSE(cable, dev, serial, clkHZ,
            invert_read_edge, verbose);
    break;

case MODE_XPCU:
    _jtag = new XilinxPlatformCableUSB(cable.vid, cable.pid, clkHZ,
        firmware_path, verbose);
    break;
```

Source: `src/jtag.cpp:126-129`, `src/jtag.cpp:214-218`.

This is the main fork:

- `digilent_hs3` -> `FtdiJtagMPSSE`
- `xilinxPlatformCableUsb` -> `XilinxPlatformCableUSB`

## Digilent HS3 path

`digilent_hs3` is FTDI MPSSE. Construction goes through:

- `Jtag::Jtag()` -> `FtdiJtagMPSSE`
- `FtdiJtagMPSSE` -> `FTDIpp_MPSSE`

`FTDIpp_MPSSE` opens the FTDI device by VID/PID and interface:

```cpp
_vid = cable.vid;
_pid = cable.pid;
...
open_device(serial, 115200);
```

Source: `src/ftdipp_mpsse.cpp:41-56`.

`FtdiJtagMPSSE::init_internal()` then switches the FTDI chip into MPSSE mode:

```cpp
if (init(5, 0xfb, BITMODE_MPSSE) != 0)
    throw std::runtime_error("low level FTDI init failed");
config_edge();
```

Source: `src/ftdiJtagMPSSE.cpp:68-98`.

JTAG transfers use FTDI MPSSE commands in `writeTDI()` and `writeTMS()`:

```cpp
int FtdiJtagMPSSE::writeTDI(const uint8_t *tdi, uint8_t *tdo, uint32_t len, bool last)
```

Source: `src/ftdiJtagMPSSE.cpp:233-350`.

In the successful log, this backend works:

```text
Jtag frequency : requested 6.00MHz    -> real 6.00MHz
found 1 devices
JTAG chain: [0]=0x04001093
...
SOJ version raw: 02 00 00 00 00 00 00
SPI RDID probe v1: 20 20 13 10
...
Read flash ... 100.00%
Done
```

Interpretation:

- The FTDI backend successfully opened the cable.
- JTAG chain detection found the Spartan-6 LX9.
- The Spartan-6 bridge bitstream loaded.
- USER4/version readback worked.
- USER1 SPI bridge readback worked.
- SPI flash JEDEC RDID returned valid bytes.
- Flash dump completed.

## Xilinx Platform Cable USB path

`xilinxPlatformCableUsb` is XPCU. Construction goes through:

- `Jtag::Jtag()` -> `XilinxPlatformCableUSB`
- `XilinxPlatformCableUSB` -> `FX2_ll`

The constructor starts here:

```cpp
XilinxPlatformCableUSB::XilinxPlatformCableUSB(const uint16_t vid,
    const uint16_t pid,
    uint32_t clkHz,
    const std::string &firmware_path,
    int8_t verbose)
```

Source: `src/xilinxPlatformCableUSB.cpp:386-398`.

For `xilinxPlatformCableUsb`, VID:PID is `03fd:0013`, so `already_initialized` is false:

```cpp
const bool already_initialized = vid == XPCU_INITIALIZED_VID &&
    pid == XPCU_INITIALIZED_PID;
...
if (!already_initialized)
    firmware_file = findXusbFirmwareFile(pid, firmware_path);
```

Source: `src/xilinxPlatformCableUSB.cpp:406-410`.

Then it creates the FX2 low-level object:

```cpp
fx2 = std::make_unique<FX2_ll>(already_initialized ? 0 : vid,
        already_initialized ? 0 : pid, XPCU_INITIALIZED_VID,
        XPCU_INITIALIZED_PID, firmware_file);
```

Source: `src/xilinxPlatformCableUSB.cpp:437-447`.

For `03fd:0013`, the selected firmware is `xusb_xp2_loader.hex`. That matches the failing log:

```text
firmware_file : ...\xusb_xp2_loader.hex
Loading firmware ... Done
Waiting reload ... 100.00%
Fail
FX2: fail to open device
JTAG init failed with: lowlevel init failed
```

Inside `FX2_ll`, the flow is:

1. Try opening the uninitialized device `03fd:0013`.
2. Claim USB interface 0.
3. Load FX2 firmware.
4. Close the original USB handle.
5. Wait for the initialized device `03fd:0008`.
6. If it cannot open `03fd:0008`, throw `FX2: fail to open device`.

Relevant code:

```cpp
dev_handle = libusb_open_device_with_vid_pid(usb_ctx, uninit_vid, uninit_pid);
...
if (!load_firmware(firmware_path)) {
    ...
}
close();
reenum = true;
...
dev_handle = libusb_open_device_with_vid_pid(usb_ctx, vid, pid);
...
if (!dev_handle) {
    progress.fail();
    throw std::runtime_error("FX2: fail to open device");
}
```

Source: `src/fx2_ll.cpp:73-124`.

This is exactly where the failing command stops. Because `Jtag` construction fails, none of the following code runs:

- `Jtag::detectChain()`
- Xilinx device claiming
- `Xilinx::prepare_flash_access()`
- `Xilinx::load_bridge()`
- `Xilinx::get_spiOverJtag_version()`
- `SPIFlash::read_id()`
- `SPIFlash::dump()`

## What would happen after XPCU initialized

If XPCU successfully reopened as `03fd:0008`, `XilinxPlatformCableUSB` would continue:

```cpp
configureXpcuUsbInterface(fx2.get());
waitForXpcuControlReady(fx2.get());
const uint16_t fx2_firmware_version = displayCableVersion();
...
enableDevice(true);
...
setClkFreq(clkHz);
...
xpcuPrimeAcceleratedTransfer(fx2.get());
```

Source: `src/xilinxPlatformCableUSB.cpp:475-518`.

For JTAG transfers, XPCU has two transport modes:

- control-transfer bitbang path
- accelerated bulk transfer path

The split is in `XilinxPlatformCableUSB::write()`:

```cpp
if (_use_control_bitbang) {
    ...
    fx2->write_ctrl(...)
    fx2->read_ctrl(...)
    ...
    return 0;
}
...
xpcuGpioTransferCtrl(...)
fx2->write(xpcu_ep_jtag_out, ...)
fx2->read(xpcu_ep_jtag_in, ...)
```

Source: `src/xilinxPlatformCableUSB.cpp:675-837`.

But again, the failing `xilinxPlatformCableUsb` log does not reach this point.

## Shared Xilinx flash path

Once JTAG exists and the FPGA is claimed, both cables use the same Xilinx flash logic.

For Spartan-6, the constructor sets:

```cpp
} else if (family == "spartan6") {
    _fpga_family = SPARTAN6_FAMILY;
    _ircode_map = ircode_mapping.at("spartan6");
}
```

Source: `src/xilinx.cpp:592-597`.

For dump-flash, Xilinx delegates to `FlashInterface`:

```cpp
if (_flash_chips & PRIMARY_FLASH) {
    select_flash_chip(PRIMARY_FLASH);
    FlashInterface::set_filename(_filename);
    if (!FlashInterface::dump(base_addr, len))
        return false;
}
```

Source: `src/xilinx.cpp:1718-1722`.

`FlashInterface::dump()` prepares bridge access, creates `SPIFlash`, then dumps:

```cpp
if (!prepare_flash_access())
    return false;
...
SPIFlash flash(this, false, _spif_verbose);
ret = flash.dump(_spif_filename, base_addr, len, _spif_rd_burst);
```

Source: `src/flashInterface.cpp:249-265`.

The bridge load is in `Xilinx::prepare_flash_access()`:

```cpp
ret = load_bridge();
...
if (get_spiOverJtag_version() == 2.0f)
    _soj_is_v2 = true;
```

Source: `src/xilinx.cpp:1019-1051`.

For Spartan-6 auto bridge selection:

```cpp
if (_fpga_family == SPARTAN6_FAMILY) {
    const std::string model = spartan6_bridge_model_from_package(_device_package);
    if (!model.empty()) {
        const std::string cor_path = bridge_dir +
            "/from_ise/spartan-6/" + model + "_spi.cor";
        if (file_exists(cor_path)) {
            bitname = cor_path;
            extension = "cor";
        }
    }
}
if (bitname.empty()) {
    bitname = bridge_dir + "/spiOverJtag_" + _device_package + ".bit.gz";
    extension = "bit";
}
```

Source: `src/xilinx.cpp:1054-1087`.

In the successful HS3 LX9 log, this selected:

```text
...\spiOverJtag_xc6slx9tqg144.bit.gz
```

The actual SPI flash file dump is:

```cpp
for (int i = 0; i < len; i += rd_burst) {
    ...
    if (0 != read(base_addr + i, (uint8_t*)&data[0], rd_burst)) {
        ...
        return false;
    }
    fwrite(data.c_str(), sizeof(uint8_t), rd_burst, fd);
}
```

Source: `src/spiFlash.cpp:324-360`.

Before dump, `SPIFlash` reads JEDEC ID:

```cpp
_spi->spi_put(0x9F, NULL, rx, 4);
...
if (jedec24 == 0x000000 || jedec24 == 0x00ffff || jedec24 == 0xffffff) {
    ...
    throw std::runtime_error("Read ID failed");
}
```

Source: `src/spiFlash.cpp:611-635`.

The HS3 log passed this step:

```text
SPI RDID probe v1: 20 20 13 10
20 20 13 10 read 20201310
```

The failing XPCU command did not reach this step.

## Why the logs are different

### Successful Digilent HS3 log

The Digilent log reaches:

```text
found 1 devices
JTAG chain: [0]=0x04001093
...
SOJ version raw: 02 00 00 00 00 00 00
SPI RDID probe v1: 20 20 13 10
...
Read flash ... 100.00%
Done
```

That means the entire path succeeded:

```text
CLI args
  -> cable_list["digilent_hs3"]
  -> MODE_FTDI_SERIAL
  -> FtdiJtagMPSSE
  -> Jtag::detectChain()
  -> Xilinx Spartan-6 object
  -> Xilinx::dumpFlash()
  -> FlashInterface::dump()
  -> Xilinx::prepare_flash_access()
  -> Xilinx::load_bridge()
  -> spiOverJtag_xc6slx9tqg144.bit.gz
  -> SPIFlash::read_id()
  -> SPIFlash::dump()
```

### Failing Xilinx Platform Cable USB log

The XPCU log reaches only:

```text
firmware_file : ...\xusb_xp2_loader.hex
Loading firmware ... Done
Waiting reload ... 100.00%
Fail
FX2: fail to open device
JTAG init failed with: lowlevel init failed
```

That means it fails here:

```text
CLI args
  -> cable_list["xilinxPlatformCableUsb"]
  -> MODE_XPCU
  -> XilinxPlatformCableUSB
  -> FX2_ll
  -> load xusb_xp2_loader.hex into 03fd:0013
  -> wait for 03fd:0008
  -> cannot open 03fd:0008
  -> throw "FX2: fail to open device"
```

It never reaches:

```text
Jtag::detectChain()
Xilinx::dumpFlash()
Xilinx::load_bridge()
SPIFlash::read_id()
SPIFlash::dump()
```

## Practical debugging implications

For `-c xilinxPlatformCableUsb`, debug USB/XPCU initialization first:

- Check whether the cable re-enumerates as `03fd:0008` after firmware load.
- Try `-c xilinxPlatformCableUsb_alt` if the physical cable initially appears as `03fd:000d`.
- Try `-c xilinxPlatformCableUsb_initialized` if the cable is already loaded as `03fd:0008`.
- On Windows, check the driver binding for the post-firmware `03fd:0008` interface. The code expects libusb access through `FX2_ll`.
- The failing log is not useful for Spartan-6 SPI bridge debugging because no JTAG scan occurred.

For `-c digilent_hs3`, the same FPGA/flash stack is proven to work on `xc6slx9tqg144`:

- JTAG chain detection works.
- `spiOverJtag_xc6slx9tqg144.bit.gz` works.
- USER4 version endpoint responds.
- USER1 SPI bridge responds.
- JEDEC RDID responds.
- Dump completes.

That successful log is a strong control case: Spartan-6 external SPI dump support is functional in the shared Xilinx codepath, at least for LX9/TQG144 with the packaged bridge and HS3 transport.

