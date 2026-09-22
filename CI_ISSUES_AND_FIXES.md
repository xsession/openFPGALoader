# CI issues and fixes

This report documents the failures observed in [GitHub Actions run 35693337542](https://github.com/xsession/openFPGALoader/actions/runs/35693337542), the follow-up [Test run 35705621385](https://github.com/xsession/openFPGALoader/actions/runs/35705621385), and the fixes applied to this checkout.

## Failure summary

| Job | Failure | Root cause | Fix |
| --- | --- | --- | --- |
| Ubuntu 22/24 build | `memset`, `memcpy`, and `memcmp` were not declared | `src/vendors/xilinx_xcf.cpp` and `src/vendors/xilinx_xcfp.cpp` used C memory functions without including their standard header | Added `#include <cstring>` to both translation units |
| Documentation | `Cannot find source directory (.../openFPGALoader/doc)` | The documentation source moved to `docs/doc`, but the workflow still used the old `doc` path | Replaced the old Sphinx job with a strict MkDocs build rooted at `docs/` |
| Documentation deployment | Potential token permission failure after the build path was corrected | The workflow did not declare write permission for `gh-pages` and release operations | Set read-only workflow permissions by default and write permissions only on the documentation and release jobs |
| Linux package test | Extraction target `/tmp/pkg/` was not created | `tar -C /tmp/pkg/` was used without creating the directory | Added `mkdir -p /tmp/pkg` |
| Windows MSYS2 clang64/ucrt64/mingw64 | CMake searched `.../deploy` and reported no `CMakeLists.txt` | The MSYS2 packaging files moved one directory deeper under `deploy/`, but the relative paths were unchanged | Updated the CMake source and license paths in `deploy/scripts/msys2/PKGBUILD` |
| macOS artifact upload | `ArtifactService/CreateArtifact` timed out after five retries | The build and archive creation succeeded; the failure was in GitHub's artifact service | Updated the artifact action major version; remote rerun still requires repository Actions write access |

## Follow-up failures from the latest Actions run

The later run exposed two independent regressions that were not covered by the
first repair:

| Jobs | Failure | Root cause | Fix |
| --- | --- | --- | --- |
| macOS and MSYS2 clang64 | Clang rejected `uint8_t buffer[length] = {}` | The SOJ code used variable-length arrays with initialization; this is accepted as a GCC extension but is ill-formed for Clang | Replaced SOJ and related diagnostic VLAs with zero-initialized `std::vector<uint8_t>` buffers and fixed the `size_t` diagnostic format string |
| Linux, macOS, and Windows cross-build checkout | `Repository not found` while fetching `externals/libwdi` | `.gitmodules` contained the misspelled owner `xsesion` | Corrected the submodule URL to `https://github.com/xsession/libwdi.git` |

The documentation job was also migrated from Sphinx to MkDocs. The new workflow
generates compatibility tables from the existing YAML data and runs
`mkdocs build --strict`, so broken internal documentation links fail CI.

## Additional refactor regressions fixed

The review also found stale paths introduced by the same directory move:

- Debian packaging now reads `deploy/packaging/linux/debian` and stages it as `DEBIAN`.
- Windows installer packaging now mounts `deploy/packaging/windows`.
- Docker Linux and Windows deployment scripts now read firmware from `deploy/ise_programmer_bins`.
- Deployment documentation and release notes now use the moved paths.
- MkDocs documentation sources are now rooted at `docs/`, with generated compatibility tables kept in sync from the YAML data.
- The Intel HEX regression test no longer depends on a hard-coded Windows developer path.

## Workflow maintenance

The first-party Actions references were updated to the currently available major versions at the time of this repair:

- `actions/checkout@v7`
- `actions/setup-python@v7`
- `actions/upload-artifact@v7`
- `actions/download-artifact@v8`

A dedicated `source-test` job now runs the Intel HEX regression test and gates releases.

## Validation performed

The following checks passed in the available runtime:

- Both workflow YAML files parse successfully.
- All deployment shell scripts pass `bash -n`.
- MkDocs compatibility generation and `mkdocs build --strict` pass.
- Both formerly failing Xilinx translation units pass GNU C++17 `-Wall -Wextra -Wpedantic -fsyntax-only`; the source no longer contains the Clang-rejected SOJ VLAs.
- Intel HEX regression tests pass.
- Moved-directory path consistency checks pass.

A full CMake build could not be executed in this environment because CMake is not installed and the managed runtime blocks package installation. The corrected workflow should therefore be validated by a new push or pull request run.

## Remaining external limitation

The original macOS artifact failure could not be rerun from this session: GitHub returned HTTP 403 (`Resource not accessible by integration`) for the rerun request. No remote branch or pull request was created; all fixes are included in this local repository and in the accompanying archive.

## SOJ source review

The SPI-over-JTAG review and source-level fixes are documented in
[`docs/SOJ_REVIEW.md`](docs/SOJ_REVIEW.md). The highest-risk defects were an
8-byte SOJ v2 probe written into 7-byte buffers, v2 detection that only worked
with verbose logging, duplicated multi-device JTAG padding, command bytes being
resent during status polling, and USER4 being accepted as an SPI fallback.
