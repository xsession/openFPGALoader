# Direct Xilinx Platform Flash programming

openFPGALoader can program Xilinx XCF Platform Flash PROMs directly through
their own JTAG interface. An FPGA is not required in the chain.

Supported devices:

- XCF01S, XCF02S and XCF04S
- XCF08P, XCF16P and XCF32P

The XCF-P implementation uses the native 16-bit IEEE 1532 instruction set,
24-bit array addressing and 32-byte programming frames. It also writes and
verifies the BTC, CCB, SUCR and DONE configuration registers.

## Wiring

Connect the programmer directly to the PROM JTAG pins:

- TCK
- TMS
- TDI
- TDO
- GND
- the cable reference-voltage pin, when the cable provides one

Power the PROM from the target board and make sure the JTAG cable I/O voltage
matches the PROM's JTAG bank voltage. The PROM must not be back-powered through
the cable.

## Detect the chain

```sh
openFPGALoader -c xilinxPlatformCableUsb_alt --detect
```

For a Digilent cable:

```sh
openFPGALoader -c digilent_hs3 --detect
```

An XCF32P is identified by IDCODE `0x05059093` with a 16-bit instruction
register. The four-bit silicon revision field is ignored.

## Program an MCS file

When the PROM is the only device in the chain:

```sh
openFPGALoader -c xilinxPlatformCableUsb_alt -f --verify image.mcs
```

When other devices are present, select the PROM's zero-based JTAG index:

```sh
openFPGALoader -c xilinxPlatformCableUsb_alt --index-chain 1 \
  -f --verify image.mcs
```

The MCS image must fit the selected PROM. XCF32P capacity is 4 MiB. Unused
bytes in the final 32-byte frame are programmed as `0xff`. The implementation
uses revision 0 and the standard slave-serial PROM configuration mode, intended
for an FPGA configured as master serial.

## Read the complete PROM

```sh
openFPGALoader -c xilinxPlatformCableUsb_alt --index-chain 0 \
  --dump-flash xcf32p-dump.bin
```

The dump operation reads the complete physical capacity: 1 MiB for XCF08P,
2 MiB for XCF16P and 4 MiB for XCF32P.

## Notes

- XCF-P devices must not enter Pause-IR or Pause-DR during ISC operations.
- Full-chip erase can take more than one minute.
- The erase sequence unlocks every physical array in the selected PROM.
- Multiple stored revisions and custom CCB configuration modes are not yet
  exposed as command-line options.