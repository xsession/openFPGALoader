# Architecture rules for contributors

These rules keep new support coherent with the existing design. They are
guidance for code review, not a requirement to force every vendor into an
identical implementation.

## Choose the owning boundary first

| If the change concerns... | Start in... |
| --- | --- |
| A command-line flag or mode | `src/main.cpp` and the relevant protocol entry point |
| A board name or default | `src/utils/board.hpp` |
| A cable name or capability | `src/utils/cable.hpp`, then `src/cables/` |
| A JTAG state/scan issue | `src/protocols/jtag.*` and `jtagInterface.*` |
| A vendor instruction sequence | `src/vendors/` |
| A file format | `src/parsers/` |
| Flash geometry or JEDEC ID | `src/protocols/spiFlashdb.hpp` |
| SOJ/BPI/XPCU runtime data | `src/transport_db/`, CMake install rules, and specialist hardware docs |
| A build or package problem | `CMakeLists.txt`, `cmake/`, `deploy/`, or `.github/workflows/` |

Avoid fixing a symptom in a neighboring layer just because it is easier to
reach. If the correct boundary is unclear, document the observed behavior and
the proposed ownership in the change description.

## Change checklists

### Board or cable support

- Add the catalog entry and define explicit defaults.
- Add or update the CMake feature gate and dependency discovery.
- Confirm `--list-boards`/`--list-cables` behavior with the intended build.
- Test a missing device, wrong cable, and normal detection path.
- Regenerate compatibility tables and update the relevant guide.

### Vendor or FPGA support

- Add IDCODE, family/model, and IR length data.
- Keep vendor instruction sequences in the vendor driver.
- Reuse the parser and generic flash service where their contracts fit.
- Cover SRAM, flash, reset, and verify behavior separately.
- Add a hardware reproduction note for timing- or probe-dependent behavior.

### Parser or flash support

- Test valid, empty, truncated, malformed, and large inputs.
- Preserve bit/byte ordering and explicit length semantics.
- Keep parser I/O independent from target I/O.
- For flash, test protection, erase boundaries, write/verify, and model lookup.

### Protocol or SOJ change

- State the bit count and TAP end state for every changed scan.
- Check buffering and flush behavior on at least one concrete adapter.
- Check leading/trailing BYPASS devices and target index handling.
- Record read/write edge assumptions.
- Update [the SOJ review](../SOJ_REVIEW.md) or a focused hardware note when
  the behavior cannot be proven in CI.

## Required validation

Run the smallest applicable set locally, then the repository checks:

```bash
python3 tests/test_intel_hex_writer.py
python3 docs/generate_compatibility.py
python3 -m mkdocs build --strict --site-dir site
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/openFPGALoader --help
./build/openFPGALoader --list-boards
./build/openFPGALoader --list-cables
```

If a required toolchain or physical fixture is unavailable, state that
limitation explicitly. A compile-only result must not be reported as hardware
validation.

## Documentation standard

Every non-obvious hardware fix should record:

- the observed failure signature;
- the affected board, cable, FPGA/flash, and clock settings;
- the root cause and the owning source boundary;
- the code or asset change;
- the checks that passed and the checks that were unavailable.

Use the architecture pages as a map, not as a substitute for source comments.
Keep comments close to the invariant they explain and link specialist
reproductions from the relevant architecture page.
