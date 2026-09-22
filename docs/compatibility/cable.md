# Cables

| Keyword | Name | Description | Notes |
| --- | --- | --- | --- |
| `anlogicCable` | [anlogic JTAG adapter](https://github.com/AnlogicInfo/anlogic-usbjtag) | JTAG adapter firmware for stm32 |  |
| `arm-usb-ocd-h` | [Olimex ARM-USB-OCD-H adapter](https://www.olimex.com/Products/ARM/JTAG/ARM-USB-OCD-H/) | High-speed 3-IN-1 fast USB ARM JTAG, USB-to-RS232 virtual port and power supply 5VDC device |  |
| `arm-usb-tiny-h` | [Olimex ARM-USB-TINY-H adapter](https://www.olimex.com/Products/ARM/JTAG/ARM-USB-TINY-H/) | Low-cost high-speed ARM USB JTAG |  |
| `bus_blaster` | [Dangerousprototypes Bus Blaster](http://dangerousprototypes.com/docs/Bus_Blaster) | Jtag adapter based on ft2232 |  |
| `bus_blaster_b` | [Dangerousprototypes Bus Blaster](http://dangerousprototypes.com/docs/Bus_Blaster) | Jtag adapter based on ft2232 (interface B) |  |
| `ch347` | [CH347](https://github.com/wuxx/USB-HS-Bridge) | CH347 is a USB HS bus converter with UART, I2C, SPI and JTAG interfaces |  |
| `ch347_jtag` | [ch347 JTAG adapter](https://www.wch-ic.com/products/CH347.html) | QinHeng Electronics USB To UART+JTAG (mode 3) |  |
| `ch552_jtag` | [ch552 JTAG adapter](https://github.com/diodep/ch55x_jtag) | Tang Nano USB-JTAG interface. FT2232C clone firmware for CH552 microcontroler |  |
| `cmsisdap` | [ARM CMSIS DAP protocol interface](https://os.mbed.com/docs/mbed-os/v6.11/debug-test/daplink.html) | ARM CMSIS DAP protocol interface |  |
| `sipeed_slogic_combo8` | [Sipeed SLogic Combo 8](https://wiki.sipeed.com/hardware/en/logic_analyzer/combo8/index.html) | The DAP-Link interface on Sipeed SLogic Combo 8 (usbbulk only) |  |
| `dfu` | [DFU interface](http://www.usb.org/developers/docs/devclass_docs/DFU_1.1.pdf) | DFU (Device Firmware Upgrade) USB device compatible with DFU protocol |  |
| `digilent` | digilent cable | FT2232 JTAG / UART cable |  |
| `digilent_b` | digilent cable | digilent FT2232 JTAG / UART cable (interface B) |  |
| `digilent_hs2` | [digilent hs2 cable](https://store.digilentinc.com/jtag-hs2-programming-cable/) | FT232H JTAG programmer cable from digilent |  |
| `digilent_hs3` | [digilent hs3](https://digilent.com/shop/jtag-hs3-programming-cable/) | JTAG programmer cable from digilent |  |
| `dirtyJtag` | [dirty Jtag](https://github.com/jeanthom/DirtyJTAG) | JTAG probe firmware for STM32F1 | Best to use release (1.4 or newer) or limit the --freq to 600000 with older releases. New version `dirtyjtag2 <https://github.com/jeanthom/DirtyJTAG/tree/dirtyjtag2>`__ is also supported |
| `ecpix5-debug` | [ecpix5-debug](https://shop.lambdaconcept.com/home/46-ecpix-5.html) | LambdaConcept ECPIX5 (45k/85k) UART/JTAG interface |  |
| `efinix_jtag_ft2232` | efinix JTAG (ft2232) | efinix JTAG interface (FTDI2232 interface B) |  |
| `efinix_spi_ft2232` | efinix SPI (ft2232) | efinix SPI interface (FTDI2232 interface A) |  |
| `efinix_jtag_ft4232` | efinix JTAG (ft4232) | efinix JTAG interface (FTDI4232 interface B) |  |
| `efinix_spi_ft4232` | efinix SPI (ft4232) | efinix SPI interface (FTDI4232 interface A) |  |
| `ft2232` | FT2232 C/D/H | generic programmer cable based on Ftdi FT2232 (interface A) |  |
| `ft2232` | [Tang Nano (1k, 4k, 8k) USB-JTAG interface](https://github.com/sipeed/RV-Debugger-BL702) | USB-JTAG/UART debugger based on BL702 microcontroler. |  |
| `ft2232` | [Sipeed RV-Debugger-BL702](https://github.com/sipeed/RV-Debugger-BL702) | RV-Debugger-BL702 is an opensource project that implement a JTAG+UART debugger with BL702C-A0. |  |
| `ft2232` | [honeycomb USB-JTAG interface.](https://github.com/Disasm/f042-ftdi) | FT2232C clone based on STM32F042 microcontroler |  |
| `ft2232_b` | FT2232 C/D/H | generic programmer cable based on Ftdi FT2232 (interface B) |  |
| `ft231X` | [FT231X](https://www.ftdichip.com/old2020/Products/ICs/FT231X.html) | generic USB<->UART converters in bitbang mode (with some limitations and workaround) |  |
| `ft232` | [FT232H](https://ftdichip.com/products/ft232hl/) | generic programmer cable based on Ftdi FT232Hx. One interface, MPSSE capable |  |
| `ft232RL` | [FT232RL](https://ftdichip.com/products/ft232rl/) | generic USB<->UART converters in bitbang mode (with some limitations and workaround) |  |
| `ft4232` | [FT4232](https://ftdichip.com/products/ft4232h-56q/) | quad interface programmer cable. MPSSE capable. |  |
| `ft4232hp` | [FT4232HP (interface A)](https://ftdichip.com/wp-content/uploads/2023/02/DS_FT4233HP.pdf) | quad interface programmer cable. MPSSE capable. High Speed USB Bridge with Type-C/PD3.0 Controller |  |
| `ft4232hp_b` | [FT4232HP (interface B)](https://ftdichip.com/wp-content/uploads/2023/02/DS_FT4233HP.pdf) | quad interface programmer cable. MPSSE capable. High Speed USB Bridge with Type-C/PD3.0 Controller |  |
| `gatemate_pgm` | [gatemate pgm](https://colognechip.com/programmable-logic/gatemate/) | Cologne Chip GateMate FPGA Programmer. FT232H-based JTAG/SPI programmer cable |  |
| `gatemate_evb_jtag` | [gatemate evb JTAG](https://colognechip.com/programmable-logic/gatemate/) | Cologne Chip GateMate JTAG programmer |  |
| `gatemate_evb_spi` | [gatemate evb spi](https://colognechip.com/programmable-logic/gatemate/) | Cologne Chip GateMate SPI programmer |  |
| `gwu2x` | [gwu2x](https://www.gowinsemi.com/en/product/detail/55/) | Gowin GWUX2X |  |
| `jetson-nano-gpio` | [Bitbang GPIO](https://github.com/jwatte/jetson-gpio-example) | Bitbang GPIO pins on Jetson Nano Linux host. Use /dev/mem to have a faster clock. |  |
| `jlink` | [jlink](https://www.segger.com/products/debug-probes/j-link) | SEGGER J-Link Debug Probes |  |
| `jlink` | jlink_base | SEGGER J-Link BASE Debug Probes |  |
| `jlink` | jtrace_pro | SEGGER J-Trace PRO Debug Probes |  |
| `jtag-smt2-nc` | [jtag-smt2-nc](https://digilent.com/shop/jtag-smt2-nc-surface-mount-programming-module) | JTAG-SMT2-NC Surface-mount Programming Module |  |
| `libgpiod` | [Bitbang GPIO](https://git.kernel.org/pub/scm/libs/libgpiod/libgpiod.git/) | Bitbang GPIO pins on Linux host. |  |
| `lpc-link2` | [lpc-link2](https://www.nxp.com/design/microcontrollers-developer-resources/lpc-link2:OM13054) | LPC-Link2 (OM13054) cmsisDAP firmware |  |
| `numato` | numato | Embedded cable for Numato Systems Mimas-A7 board |  |
| `orbtrace` | [orbtrace interface](https://github.com/orbcode/orbtrace) | Open source FPGA-based debug and trace interface |  |
| `papilio` | [papilio](https://papilio.cc/) | Papilio FPGA Platform |  |
| `remote-bitgang` | [OpenOCD remote bitbang](https://github.com/openocd-org/openocd/blob/master/doc/manual/jtag/drivers/remote_bitbang.txt) | The remote_bitbang JTAG driver is used to drive JTAG from a remote (TCP) process |  |
| `steppenprobe` | [steppenprobe](https://github.com/diegoherranz/steppenprobe) | Open Source Hardware JTAG/SWD/UART/SWO interface board based on FTDI FT2232H |  |
| `tigard` | [tigard](https://www.crowdsupply.com/securinghw/tigard) | SWD/JTAG/UART/SPI programmer based on Ftdi FT2232HQ |  |
| `usb-blaster` | intel USB Blaster I interface | JTAG programmer cable from intel/altera (FT245 + EPM7064) |  |
| `usb-blasterII` | [intel USB Blaster II interface](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/ug/ug_usb_blstr_ii_cable.pdf) | JTAG programmer cable from intel/altera (EZ-USB FX2 + EPM570) |  |
| `usb-blasterIII` | [intel USB Blaster III interface](https://www.intel.com/content/dam/www/programmable/us/en/pdfs/literature/ug/ug_usb_blstr_ii_cable.pdf) | JTAG programmer cable from intel/altera (FTDI2232 with custom VID/PID) |  |
| `xilinxPlatformCableUsb` | [Xilinx Platform Cable USB (XPCU)](https://www.amd.com/en/products/adaptive-socs-and-fpgas/board-accessories/hw-usb-ii-g.html) | Xilinx Platform Cable USB (XPCU) from AMD/Xilinx |  |
| `xilinxPlatformCableUsb_alt` | [Xilinx Platform Cable USB (XPCU). Alternate VID/PID (found in SP601).](https://www.amd.com/en/products/adaptive-socs-and-fpgas/board-accessories/hw-usb-ii-g.html) | Xilinx Platform Cable USB (XPCU) from AMD/Xilinx |  |
| `xvc-client` | [Xilinx Virtual Cable](https://github.com/Xilinx/XilinxVirtualCable) | Xilinx Virtual Cable (XVC) is a TCP/IP-based protocol that acts like a JTAG cable. |  |
| `xvc-server` | [Xilinx Virtual Cable (server side)](https://github.com/Xilinx/XilinxVirtualCable) | Xilinx Virtual Cable (XVC) is a TCP/IP-based protocol that acts like a JTAG cable. |  |

### Status values

- **AS**: Active Serial flash mode
- **EF**: External Flash
- **IF**: Internal Flash
- **NA**: Not Available
- **NT**: Not Tested
