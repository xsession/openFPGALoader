#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-docker-windows}"
PREFIX_DIR="${PREFIX_DIR:-${ROOT_DIR}/dist/docker-windows/install}"
PACKAGE_DIR="${PACKAGE_DIR:-${ROOT_DIR}/dist/docker-windows}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
TARGET_TRIPLE="${TARGET_TRIPLE:-x86_64-w64-mingw32}"
CROSS_PREFIX="${CROSS_PREFIX:-/opt/x86_64-w64-mingw32}"
TOOLCHAIN_FILE="${CMAKE_TOOLCHAIN_FILE:-/opt/toolchain-mingw64.cmake}"
CMAKE_EXTRA_ARGS="${CMAKE_EXTRA_ARGS:-}"

# openFPGALoader is a command-line tool. Force the Windows PE subsystem to
# console/CUI so stdout/stderr are visible in cmd.exe and PowerShell. This also
# protects us if an upstream CMake change accidentally marks the target WIN32.
WINDOWS_CONSOLE_LINK_FLAGS="${WINDOWS_CONSOLE_LINK_FLAGS:--static -static-libgcc -static-libstdc++ -Wl,--subsystem,console}"

if [[ "${1:-}" == "--clean" ]]; then
  rm -rf "${BUILD_DIR}" "${PREFIX_DIR}"
  shift
fi

mkdir -p "${BUILD_DIR}" "${PREFIX_DIR}" "${PACKAGE_DIR}"

export PKG_CONFIG_LIBDIR="${CROSS_PREFIX}/lib/pkgconfig"
export PKG_CONFIG_PATH="${CROSS_PREFIX}/lib/pkgconfig"

pkg-config --exists libftdi1
pkg-config --exists libusb-1.0
pkg-config --exists hidapi

# zlib's current CMake install names the static Windows library libzs.a, but
# pkg-config/CMake may still ask the final linker for -lz. Ensure the expected
# static archive name exists before configuring openFPGALoader.
if [[ -f "${CROSS_PREFIX}/lib/libzs.a" && ! -f "${CROSS_PREFIX}/lib/libz.a" ]]; then
  cp "${CROSS_PREFIX}/lib/libzs.a" "${CROSS_PREFIX}/lib/libz.a"
  "${TARGET_TRIPLE}-ranlib" "${CROSS_PREFIX}/lib/libz.a" || true
fi

if [[ ! -f "${CROSS_PREFIX}/lib/libz.a" ]]; then
  echo "ERROR: static zlib archive not found: ${CROSS_PREFIX}/lib/libz.a" >&2
  echo "       zlib may have installed libzs.a only; rebuild the Docker image with the fixed Dockerfile." >&2
  exit 1
fi

# shellcheck disable=SC2086
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN_FILE}" \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}" \
  -DCMAKE_PREFIX_PATH="${CROSS_PREFIX}" \
  -DPKG_CONFIG_EXECUTABLE=/usr/bin/pkg-config \
  -DOPENFPGALOADER_PORTABLE_WINDOWS_DATADIR=ON \
  -DCMAKE_WIN32_EXECUTABLE=OFF \
  -DCMAKE_EXE_LINKER_FLAGS="${WINDOWS_CONSOLE_LINK_FLAGS}" \
  ${CMAKE_EXTRA_ARGS} \
  "$@"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
cmake --install "${BUILD_DIR}"

# Copy Xilinx Platform Cable USB FX2 firmware into the portable package.
# The repository is bind-mounted at /src by Docker Compose, so locally supplied
# ISE firmware is available here at runtime (but not during Docker image build).
mkdir -p "${PREFIX_DIR}/share/openFPGALoader"

LOCAL_FIRMWARE_DIR="${ROOT_DIR}/ise_programmer_bins"

if compgen -G "${LOCAL_FIRMWARE_DIR}/*.hex" >/dev/null; then
  cp -f "${LOCAL_FIRMWARE_DIR}/"*.hex \
    "${PREFIX_DIR}/share/openFPGALoader/"
  echo "Copied Xilinx firmware from ${LOCAL_FIRMWARE_DIR}"
elif compgen -G "${CROSS_PREFIX}/share/openFPGALoader/*.hex" >/dev/null; then
  cp -f "${CROSS_PREFIX}/share/openFPGALoader/"*.hex \
    "${PREFIX_DIR}/share/openFPGALoader/"
fi

if [[ ! -f "${PREFIX_DIR}/share/openFPGALoader/xusb_emb.hex" ]]; then
  echo "WARNING: xusb_emb.hex is not installed." >&2
  echo "         xilinxPlatformCableUsb_alt may fail unless OPENFPGALOADER_XUSB_FIRMWARE is set." >&2
fi

EXE="${PREFIX_DIR}/bin/openFPGALoader.exe"

if [[ ! -f "${EXE}" ]]; then
  echo "ERROR: expected executable not found: ${EXE}" >&2
  exit 1
fi

if command -v "${TARGET_TRIPLE}-objdump" >/dev/null 2>&1; then
  SUBSYSTEM_LINE="$("${TARGET_TRIPLE}-objdump" -p "${EXE}" | grep -i "Subsystem" | head -n1 || true)"
  echo "PE ${SUBSYSTEM_LINE}"

  echo "PE DLL imports:"
  "${TARGET_TRIPLE}-objdump" -p "${EXE}" | sed -n 's/^\tDLL Name: /  /p' || true

  if echo "${SUBSYSTEM_LINE}" | grep -qi "Windows GUI"; then
    echo "ERROR: ${EXE} was linked as Windows GUI subsystem, so console output will be hidden." >&2
    echo "       Expected Windows CUI/console subsystem." >&2
    exit 1
  fi
fi

# Copy every non-system DLL imported by the EXE. A Windows CLI can look like it
# produces no output when it exits before main() because a runtime DLL is missing.
# Static linking is requested above, but this keeps the package robust if a future
# dependency switches back to a DLL import library.
copy_dll_if_found() {
  local dll="$1"
  local found=""

  case "${dll,,}" in
    kernel32.dll|user32.dll|gdi32.dll|winspool.drv|shell32.dll|ole32.dll|oleaut32.dll|uuid.dll|comdlg32.dll|advapi32.dll|setupapi.dll|cfgmgr32.dll|hid.dll|version.dll|msvcrt.dll|ws2_32.dll|bcrypt.dll|ntdll.dll)
      return 0
      ;;
  esac

  for base in \
    "${PREFIX_DIR}/bin" \
    "${CROSS_PREFIX}/bin" \
    "${CROSS_PREFIX}/lib" \
    "/usr/${TARGET_TRIPLE}/bin" \
    "/usr/lib/gcc/${TARGET_TRIPLE}" \
    "/usr"; do

    if [[ -d "${base}" ]]; then
      found="$(find "${base}" -name "${dll}" -print -quit 2>/dev/null || true)"
      if [[ -n "${found}" ]]; then
        cp -f "${found}" "${PREFIX_DIR}/bin/"
        echo "Copied DLL dependency: ${dll}"
        return 0
      fi
    fi
  done

  echo "WARNING: could not find non-system DLL dependency: ${dll}" >&2
}

if command -v "${TARGET_TRIPLE}-objdump" >/dev/null 2>&1; then
  while IFS= read -r dll; do
    [[ -n "${dll}" ]] && copy_dll_if_found "${dll}"
  done < <("${TARGET_TRIPLE}-objdump" -p "${EXE}" | sed -n 's/^\tDLL Name: //p')
fi

VERSION="$("${EXE}" --version 2>/dev/null | head -n1 | tr -cs 'A-Za-z0-9._-' '-' | sed 's/^-//;s/-$//' || true)"

if [[ -z "${VERSION}" ]]; then
  VERSION="local"
fi

ARCHIVE="${PACKAGE_DIR}/openFPGALoader-windows-x86_64-${VERSION}.zip"

rm -f "${ARCHIVE}" "${ARCHIVE}.sha256"

(
  cd "${PREFIX_DIR}"
  zip -r "${ARCHIVE}" .
)

sha256sum "${ARCHIVE}" > "${ARCHIVE}.sha256"

echo "Built ${EXE}"
echo "Package: ${ARCHIVE}"
echo "Checksum: ${ARCHIVE}.sha256"
