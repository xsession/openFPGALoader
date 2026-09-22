# SPI-over-JTAG (SOJ) source review

Reviewed against the checkout at `949bab1` (`v.1.1.3`), with the fixes in this
working tree applied afterward. The review covers the C++ transport, the JTAG
chain wrapper, the SOJ RTL, and the optional diagnostic tool.

## Findings and fixes

| Priority | Area | Finding | Fix in this checkout |
| --- | --- | --- | --- |
| Critical | `src/vendors/xilinx.cpp`, `get_spiOverJtag_version()` | The v2 probe computed an 8-byte transfer (`v2_real_len + 2`) into 7-byte `v2_pkt` and `v2_jrx` arrays. This could overwrite the stack during v2 detection. | Use zero-initialized vectors sized to the exact transfer length. |
| Critical | v2 detection | Returning `2.0f` was conditional on `_verbose > 0`; normal users silently fell back to v1 even when the probe found v2 data. | Keep logging conditional, but make detection independent of verbosity. |
| High | JTAG chain setup | Version probing manually added bypass bits even though `Jtag::shiftDR()` already inserts `_dr_bits_before`. Multi-device chains therefore received duplicated padding. | Removed the second, manual padding scan. |
| High | v1/v2 transfers | Read-only transfers left MOSI bytes and the SOJ packet tail uninitialized. Results depended on stack contents and could send arbitrary SPI bits. | Zero-initialize v1 and v2 TX/RX buffers, including the extra packet byte. |
| High | `spi_wait()` | The initial header+command buffer was resent for every status poll. In v2 this reissued the SOJ header/flash command instead of clocking dummy bytes, breaking erase/program polling. | Send the command once, then use `0xff 0xff` dummy bytes for polling and exit. |
| High | USER fallback | RDID fallback probed and could select USER4. In the bundled wrapper USER4 is the bridge version interface, not the SPI path. | Restrict fallback candidates to USER1–USER3. |
| Medium | v2 length boundary | `real_len == 32` was encoded in short mode even though the 5-bit short length field cannot represent 32. | Select extended mode for `real_len >= 32`. |
| Medium | diagnostic tool | The tool used a nonexistent `Part` type, hardcoded `0x1e` as USER1, and allocated VLAs. | Read IR length from the selected `Jtag` device, use Xilinx USER1 `0x02` for its documented 7-series/Spartan-6 scope, and use vectors. |
| Medium | JTAG target selection | `device_select()` and the CLI accepted an index equal to the device count, leading to an out-of-range target lookup. | Reject `index >= device_count` before selecting the target. |

The bundled RTL assigns USER4 to the version interface in
[`xilinx_spiOverJtag.v`](https://github.com/xsession/openFPGALoader/blob/master/src/transport_db/spiOverJtag/xilinx_spiOverJtag.v).
This is also consistent with AMD's [BSCANE2 documentation](https://docs.amd.com/r/en-US/ug953-vivado-7series-libraries/BSCANE2),
which defines USER1–USER4 as separate JTAG user chains.

## Remaining SOJ risks requiring hardware or RTL simulation

### RX alignment is still not proven

The current C++ path extracts short-packet data at `idx = 2` and long-packet
data at `idx = 3`. The diagnostic tool documents three RX header bytes and
tests `idx = 3` for short packets. Historical SOJ code also reconstructed a
chain-dependent bit shift, while the current `Jtag::shiftDR()` automatically
handles bypass devices before the selected target. These are incompatible
assumptions, so changing the offset again without a capture could fix one
board and break another.

Required next test:

1. Capture a short RDID transfer from a known-good single-device Artix-7/HS3
   setup and a long read crossing the 32-byte boundary.
2. Repeat with a multi-device chain.
3. Record the raw TDI/TDO bit stream and assert the expected data offset in a
   golden-vector test.

The relevant implementation is in
[`xilinx.cpp`](https://github.com/xsession/openFPGALoader/blob/master/src/vendors/xilinx.cpp), and the chain behavior is in
[`jtag.cpp`](https://github.com/xsession/openFPGALoader/blob/master/src/protocols/jtag.cpp).

### Detection heuristics are permissive

`looks_like_valid_jedec_reply()` rejects only all-zero/all-`0xff` and a few
edge-byte patterns. A noisy USER chain can still look valid. The safer design
is to require a known JEDEC manufacturer or a repeated identical RDID response
before changing `_user_instruction` or promoting `_soj_is_v2`.

### The diagnostic is not yet a CLI feature

`run_spi_debug()` is conditionally compiled by `ENABLE_SPI_DEBUG`, but there
is no `main.cpp` option or call site that exposes the documented
`--spi-debug` command. It is currently a buildable helper, not an end-to-end
user command.

### RTL test coverage is absent

`spiOverJtag_core.v` has the expected short/extended header states and an
intentional infinite-loop mode, but there is no checked-in SOJ testbench or
golden packet test. The header comments also do not consistently match the
actual bit fields. Add an RTL simulation that checks lengths 0, 1, 31, 32,
and a long read/write, including USER4 version capture.

## Validation available in this environment

- `g++ -std=gnu++17 -Wall -Wextra -Wpedantic -DDATA_DIR=... -fsyntax-only src/vendors/xilinx.cpp` — passed; existing C++20 designated-initializer warnings remain in `spiFlashdb.hpp`.
- The optional diagnostic translation unit passes the same syntax check.
- `python3 tests/test_intel_hex_writer.py` — all tests passed.
- `git diff --check` — passed.
- Full CMake build, RTL simulation, and physical HS3/XPCU transfers were not available because CMake/Verilog simulators and JTAG hardware are absent in this runtime.

For comparison with the upstream implementation, see the
[SOJ implementation at commit `24e46d1`](https://github.com/xsession/openFPGALoader/blob/24e46d13bb8f2bc9371e9ca8443ece2fafc4b20d/src/xilinx.cpp)
and the upstream
[SOJ RTL](https://github.com/xsession/openFPGALoader/blob/master/spiOverJtag/spiOverJtag_core.v).
