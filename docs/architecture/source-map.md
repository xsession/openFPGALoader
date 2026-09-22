# Source map and ownership

Use this page before editing. It maps user-visible behavior to the smallest
reasonable source boundary and identifies the neighboring contracts that must
be checked.

## Repository map

| Area | Main paths | Owns | Usually does not own |
| --- | --- | --- | --- |
| CLI | `src/main.cpp`, `src/utils/display.*` | Option parsing, defaults, mode dispatch, user-facing status | Vendor instruction details or USB transactions |
| Core protocols | `src/protocols/` | JTAG TAP, flash algorithms, protocol interfaces | Cable enumeration and vendor-specific command sequences |
| Cable adapters | `src/cables/` | Probe discovery, USB/HID/GPIO/DFU/XVC I/O, low-level timing | FPGA family configuration algorithms |
| Vendor devices | `src/vendors/` | IDCODE-specific programming, reset, configuration, vendor flash paths | Generic file decoding or probe enumeration |
| Parsers | `src/parsers/` | File format recognition and bitstream extraction | Selecting a cable or changing target hardware |
| Hardware catalogs | `src/utils/board.hpp`, `cable.hpp`, `part.hpp`, `spiFlashdb.hpp` | Names, IDs, defaults, capabilities, flash geometry | Dynamic discovery of arbitrary hardware |
| Transport assets | `src/transport_db/`, `deploy/ise_programmer_bins/` | SOJ/BPI bitstreams and XPCU firmware | C++ protocol sequencing |
| Build and packaging | `CMakeLists.txt`, `cmake/`, `deploy/`, `.github/workflows/` | Feature selection, dependency discovery, install layout, archives | Runtime target behavior |
| Documentation and generated support lists | `docs/`, `docs/generate_compatibility.py` | Operator guidance, design records, compatibility tables | Being the only source of hardware truth |

## How a behavior is resolved

For a request such as `openFPGALoader -b arty design.bit`, inspect in this
order:

1. **Option semantics:** locate the option in `parse_opt()` in `src/main.cpp`.
2. **Board defaults:** find `arty` in `board_list` in `src/utils/board.hpp`.
3. **Cable implementation:** follow the resulting cable name into
   `src/utils/cable.hpp` and the matching file in `src/cables/`.
4. **Target resolution:** follow the FPGA IDCODE through `src/utils/part.hpp`
   into the vendor class selected by `main.cpp`.
5. **File path:** follow the extension or explicit `--file-type` into
   `src/parsers/`.
6. **Persistent programming:** follow the vendor's `FlashInterface` calls into
   `SPIFlash`, `BPIFlash`, or a vendor-specific flash path.
7. **Runtime assets:** check `CMakeLists.txt` install rules and the data-path
   lookup if the operation needs SOJ, BPI, or probe firmware files.

## Compile-time capability model

The executable is a configured product, not a runtime plugin host. CMake
defines such as `ENABLE_XILINX_SUPPORT`, `ENABLE_CMSISDAP_V1`, `USE_LIBFTDI`,
`ENABLE_DFU`, and `ENABLE_XVC_SERVER` control both source inclusion and
available catalog entries. When adding a feature, check all three layers:

| Layer | Question |
| --- | --- |
| Configuration | Does `CMakeLists.txt` expose a clear option and discover dependencies safely? |
| Compilation | Are the source files included only when their required definitions and libraries exist? |
| Runtime | Does `--list-*`, board selection, and failure reporting reflect the configured capability? |

Platform-specific decisions are also part of the product surface. Linux can
use udev/libgpiod and remote bitbang support; Windows cross-builds use a
portable data directory and exclude Linux-only integrations; macOS disables
Linux device-management features in the workflow.

## Extension points

### Add a board

Update the board catalog with the board name, communication mode, cable,
device/part, pins, default clock, and flash metadata. Then regenerate the
compatibility page, build, and smoke-test `--list-boards` plus the real board
sequence. See [Adding FPGA and flash support](../adding-new-fpga-and-flash-guide.md).

### Add a cable

Implement `JtagInterface` for JTAG transports or `FlashInterface` for direct
SPI. Register the cable in `src/utils/cable.hpp`, add the CMake feature gate,
wire dependency discovery, and verify enumeration, clock configuration,
buffer flushing, read-edge behavior, and error paths.

### Add a vendor/device

Add the IDCODE and IR length to the part catalog, implement the `Device`
contract, and connect the vendor class in `src/main.cpp` behind the matching
CMake definition. Keep parsers and generic flash algorithms reusable unless
the device protocol genuinely differs.

### Add a file format

Implement `ConfigBitstreamParser`, register the explicit file-type path, and
test malformed input, empty input, byte/word ordering, gzip handling, and
large files. The parser must not open USB devices or mutate target state.

### Add a flash model

Extend `spiFlashdb.hpp` with JEDEC identity, geometry, erase capabilities,
protection behavior, and quad-mode details. Add a regression fixture where
possible and document any operation that requires physical validation.

### Change SOJ or XPCU behavior

Treat it as a cross-layer change: inspect the JTAG scan alignment, the
transport-specific adapter hooks, the generated bitstream/firmware asset, the
install path, and the hardware reproduction notes. Start with
[the SOJ review](../SOJ_REVIEW.md) and keep the failure signature and
diagnostic evidence in the documentation.

## Invariants worth preserving

- JTAG bit lengths are explicit; do not silently convert bit counts to byte
  counts at the protocol boundary.
- The selected chain index is meaningful after detection and must remain
  stable when passive or BYPASS devices are represented.
- Buffered adapters must flush at the boundary expected by the transport;
  changing batching can alter both performance and hardware timing.
- Installed transport data must be found from the configured data directory,
  including portable Windows packages.
- User-facing errors should identify the selected board/cable/device and the
  next useful diagnostic command whenever that information is available.
