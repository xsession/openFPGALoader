#!/usr/bin/env python3
"""Ad-hoc verification: Intel HEX writer matches the C++ implementation in dumpFlash()."""

def write_intel_hex(data):
    """Replicate the FIXED C++ Intel HEX writer from xilinx.cpp dumpFlash()."""
    lines = []

    # Initial Extended Linear Address record (base = 0x0000)
    lines.append(":020000040000FA\n")

    offset = 0
    while offset < len(data):
        remaining = len(data) - offset
        count = min(remaining, 16)
        addr = offset & 0xFFFF

        # Compute checksum
        checksum = 0
        checksum = (checksum + count) & 0xFF
        checksum = (checksum + (addr >> 8)) & 0xFF
        checksum = (checksum + (addr & 0xFF)) & 0xFF
        # Record type 00
        checksum = (checksum + 0) & 0xFF
        for i in range(count):
            checksum = (checksum + data[offset + i]) & 0xFF
        checksum = (~checksum + 1) & 0xFF

        line = ":{:02X}{:04X}00".format(count, addr)
        for i in range(count):
            line += "{:02X}".format(data[offset + i])
        line += "{:02X}\n".format(checksum)
        lines.append(line)
        offset += count

        # If we just crossed a 64 KB boundary, emit a new ELA record
        if offset < len(data) and (offset & 0xFFFF) == 0:
            segment = (offset >> 16) & 0xFFFF
            cs = 0
            cs = (cs + 2) & 0xFF
            cs = (cs + 0) & 0xFF
            cs = (cs + 0) & 0xFF
            cs = (cs + 4) & 0xFF
            cs = (cs + (segment >> 8)) & 0xFF
            cs = (cs + (segment & 0xFF)) & 0xFF
            cs = (~cs + 1) & 0xFF
            lines.append(":02000004{:04X}{:02X}\n".format(segment, cs))

    # EOF record
    lines.append(":00000001FF\n")
    return "".join(lines)

def verify_checksum(line):
    """Verify an Intel HEX line checksum."""
    line = line.strip()
    if not line.startswith(":"):
        return False
    hexdata = line[1:]
    values = [int(hexdata[i:i+2], 16) for i in range(0, len(hexdata), 2)]
    return sum(values) & 0xFF == 0

def test_basic():
    """Test with known data."""
    data = bytes(range(32))
    hex_out = write_intel_hex(data)

    lines = hex_out.strip().split("\n")
    assert len(lines) == 4, f"Expected 4 lines (1 ELA + 2 data + EOF), got {len(lines)}"

    assert lines[0] == ":020000040000FA", f"Bad ELA: {lines[0]}"
    assert verify_checksum(lines[0]), "ELA checksum failed"
    assert lines[1].startswith(":10000000"), f"Bad first record: {lines[1]}"
    assert verify_checksum(lines[1])
    assert lines[2].startswith(":10001000"), f"Bad second record: {lines[2]}"
    assert verify_checksum(lines[2])
    assert lines[3] == ":00000001FF", f"Bad EOF: {lines[3]}"
    assert verify_checksum(lines[3])
    print("PASS: 32-byte sequential data")

def test_single_byte():
    data = bytes([0xAB])
    hex_out = write_intel_hex(data)
    lines = hex_out.strip().split("\n")
    assert len(lines) == 3, f"Expected 3 lines, got {len(lines)}"
    assert lines[0] == ":020000040000FA"
    assert lines[1].startswith(":01000000"), f"Bad record: {lines[1]}"
    assert verify_checksum(lines[1])
    assert "AB" in lines[1]
    print("PASS: single byte")

def test_64kb_boundary():
    """Test data exactly at 64KB boundary (triggers ELA)."""
    data = bytes([0xFF] * 0x10000)  # exactly 64KB = 1 segment, no extra ELA needed
    hex_out = write_intel_hex(data)
    lines = hex_out.strip().split("\n")
    # 4096 data lines + 1 initial ELA + 1 EOF = 4098
    assert len(lines) == 4098, f"Expected 4098 lines, got {len(lines)}"
    assert lines[0] == ":020000040000FA"
    assert lines[-1] == ":00000001FF"
    assert lines[1].startswith(":10000000")
    for line in lines[1:-1]:
        assert verify_checksum(line), f"Checksum failed: {line}"
    print("PASS: 64KB exactly (no extra ELA)")

def test_64kb_plus():
    """Test data crossing 64KB boundary (triggers extra ELA)."""
    data = bytes([0xFF] * (0x10000 + 16))  # 64KB + 16 bytes = 4097 data records
    hex_out = write_intel_hex(data)
    lines = hex_out.strip().split("\n")
    # 4097 data + 1 initial ELA + 1 extra ELA + 1 EOF = 4100
    assert len(lines) == 4100, f"Expected 4100 lines, got {len(lines)}"
    # Find the extra ELA
    elas = [l for l in lines if l.startswith(":02")]
    assert len(elas) == 2, f"Expected 2 ELA records, got {len(elas)}"
    assert elas[0] == ":020000040000FA"
    assert elas[1] == ":020000040001F9"  # segment = 0x0001
    assert verify_checksum(elas[1])
    print("PASS: 64KB + 16 bytes (extra ELA at boundary)")

def test_3_segments():
    """Test data spanning 3 segments."""
    data = bytes([0xAA] * (0x10000 * 2 + 100))  # 2 full segments + 100 bytes
    hex_out = write_intel_hex(data)
    lines = hex_out.strip().split("\n")
    elas = [l for l in lines if l.startswith(":02")]
    assert len(elas) == 3, f"Expected 3 ELA records, got {len(elas)}"
    assert elas[0] == ":020000040000FA"
    assert elas[1] == ":020000040001F9"
    assert elas[2] == ":020000040002F8"
    print("PASS: 3 segments")

def test_roundtrip():
    """Verify data can be recovered from generated HEX."""
    data = bytes([0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0xFF] * 8)  # 48 bytes
    hex_out = write_intel_hex(data)

    # Parse back
    recovered = bytearray()
    current_base = 0
    for line in hex_out.strip().split("\n"):
        if not line.startswith(":"):
            continue
        count = int(line[1:3], 16)
        rec_type = int(line[7:9], 16)
        if rec_type == 0:
            hex_bytes = line[9:9 + count*2]
            for i in range(0, len(hex_bytes), 2):
                recovered.append(int(hex_bytes[i:i+2], 16))
        elif rec_type == 4:
            hex_bytes = line[9:9 + count*2]
            segment = int(hex_bytes, 16)
            current_base = segment << 16

    assert bytes(recovered) == data, f"Roundtrip mismatch: {len(recovered)} vs {len(data)} bytes"
    print("PASS: roundtrip data recovery")

def test_matches_reference_format():
    """Verify reference file format and line count."""
    REF = "C:/Users/livanyi/Desktop/WORK/GIT/openFPGALoader/dist/docker-windows/install/bin/rb_1.mcs"
    with open(REF, "rb") as f:
        ref_bytes = f.read(500)
    ref_str = ref_bytes.decode("ascii")

    assert ref_str.startswith(":020000040000FA\n"), "Reference should start with ELA record + LF"
    assert b"\r" not in ref_bytes, "Reference should use LF-only, not CRLF"
    assert b"\n\n" not in ref_bytes, "Reference should not have blank lines between records"

    # Verify our output uses same format
    sample = write_intel_hex(bytes([0xFF] * 16))
    assert "\r" not in sample, "Our output should use LF-only"
    assert "\n\n" not in sample, "Our output should not have blank lines"
    assert sample.startswith(":020000040000FA\n"), "Our output should start with ELA"

    print("PASS: matches reference file format")

if __name__ == "__main__":
    print("=== Intel HEX writer verification (FIXED v2) ===")
    test_basic()
    test_single_byte()
    test_64kb_boundary()
    test_64kb_plus()
    test_3_segments()
    test_roundtrip()
    test_matches_reference_format()
    print("ALL TESTS PASSED")