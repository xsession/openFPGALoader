#!/usr/bin/env python3
"""Regression checks for spiOverJtag v2 JTAG-chain receive alignment."""


def reverse_byte(value: int) -> int:
    value &= 0xFF
    return int(f"{value:08b}"[::-1], 2)


def decode_shifted_jtag_byte(data: bytes, shift: int) -> int:
    if shift == 0:
        return reverse_byte(data[0])
    value = reverse_byte(data[0] >> shift)
    if shift == 1:
        value |= data[1] & 0x01
    else:
        value |= reverse_byte(data[1]) >> (8 - shift)
    return value


def decode_stream(raw: bytes, length: int, shift: int, start: int) -> bytes:
    return bytes(
        decode_shifted_jtag_byte(raw[start + i : start + i + 2], shift)
        for i in range(length)
    )


def main() -> None:
    # Captured from Artix-7 XC7A200T + Digilent HS3 + SOJ 2.00.
    # SOJ short-packet header occupies raw bytes 0..1; flash data begins at 2.
    raw = bytes.fromhex("ff ff 09 ba 30 11 00")
    decoded = decode_stream(raw, 3, shift=1, start=2)
    assert decoded == bytes.fromhex("20 ba 19"), decoded.hex(" ")

    # This reproduces the pre-fix v2 decoder and documents the regression.
    old_decode = bytes(reverse_byte(raw[2 + i]) for i in range(4))
    assert old_decode == bytes.fromhex("90 5d 0c 88"), old_decode.hex(" ")

    print("SOJ v2 alignment regression: PASS (09 ba 30 11 -> 20 ba 19)")


if __name__ == "__main__":
    main()
