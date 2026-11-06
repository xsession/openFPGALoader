# SOJ v2 MT25QL256 RDID alignment bug

## Symptom

On an Artix-7 XC7A200T accessed through a Digilent HS3, SOJ 2.00 correctly loads but external-flash detection reports a nonsensical ID such as `0x905d0c88` instead of the expected Micron JEDEC ID.

Observed SOJ v2 receive bytes after the packet header:

```text
09 ba 30 11
```

The pre-fix SOJ v2 decoder bit-reversed each byte independently, producing:

```text
90 5d 0c 88
```

## Root cause

The JTAG scan stream is offset by the bypass bits contributed by the chain. For the reported one-device chain, `_jtag_chain_len == 1`, so each returned SPI byte spans two adjacent JTAG bytes. SOJ v1 and the version-probe path already use cross-byte alignment logic, but `spi_put_v2()` previously did not.

Applying the existing `decode_shifted_jtag_stream()` helper with `shift=1` yields:

```text
09 ba -> 20
ba 30 -> ba
30 11 -> 19
```

Therefore the flash is correctly responding with:

```text
20 ba 19
```

## Device identification

For Micron MT25QL256ABA, READ ID (`0x9f`) returns:

- manufacturer: `0x20`
- memory type: `0xba` (3 V)
- density: `0x19` (256 Mbit / 32 MiB)

The canonical 24-bit JEDEC ID is therefore `0x20ba19`.

The former `0xba2119` entry was incorrect and has been removed. The existing `0x20ba19` entry now identifies both N25Q256 and MT25QL256A-compatible parts.

## Fix

`Xilinx::spi_put_v2()` now decodes returned data with:

```cpp
decode_shifted_jtag_stream(jrx.data(), rx, len,
    _jtag_chain_len, idx);
```

instead of independently reversing each returned byte.

## Regression test

Run:

```bash
python3 tests/test_soj_v2_alignment.py
```

Expected:

```text
SOJ v2 alignment regression: PASS (09 ba 30 11 -> 20 ba 19)
```

This fix is required not only for RDID; all SOJ v2 receive transfers, including flash reads, need the same chain-aware alignment.
