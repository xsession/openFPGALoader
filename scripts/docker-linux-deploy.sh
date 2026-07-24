#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-docker-linux}"
PREFIX_DIR="${PREFIX_DIR:-${ROOT_DIR}/dist/docker-linux/install}"
PACKAGE_DIR="${PACKAGE_DIR:-${ROOT_DIR}/dist/docker-linux}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
CMAKE_EXTRA_ARGS="${CMAKE_EXTRA_ARGS:-}"

if [[ "${1:-}" == "--clean" ]]; then
  rm -rf "${BUILD_DIR}" "${PREFIX_DIR}"
  shift
fi

mkdir -p "${BUILD_DIR}" "${PREFIX_DIR}" "${PACKAGE_DIR}"

# shellcheck disable=SC2086
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -G Ninja \
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
  -DCMAKE_INSTALL_PREFIX="${PREFIX_DIR}" \
  ${CMAKE_EXTRA_ARGS} \
  "$@"

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"
cmake --install "${BUILD_DIR}"

EXE="${PREFIX_DIR}/bin/openFPGALoader"

if [[ ! -f "${EXE}" ]]; then
  echo "ERROR: expected executable not found: ${EXE}" >&2
  exit 1
fi

mkdir -p "${PREFIX_DIR}/share/openFPGALoader"

LOCAL_FIRMWARE_DIR="${ROOT_DIR}/ise_programmer_bins"

if compgen -G "${LOCAL_FIRMWARE_DIR}/*.hex" >/dev/null; then
  cp -f "${LOCAL_FIRMWARE_DIR}/"*.hex \
    "${PREFIX_DIR}/share/openFPGALoader/"
  echo "Copied Xilinx firmware from ${LOCAL_FIRMWARE_DIR}"
fi

mkdir -p "${PREFIX_DIR}/lib/udev/rules.d"
for rules in "${ROOT_DIR}/70-openfpgaloader.rules" "${ROOT_DIR}/99-openfpgaloader.rules"; do
  if [[ -f "${rules}" ]]; then
    cp -f "${rules}" "${PREFIX_DIR}/lib/udev/rules.d/"
  fi
done

mkdir -p "${PREFIX_DIR}/share/doc/openFPGALoader"
{
  echo "openFPGALoader Linux Docker deployment"
  echo
  echo "Built on: $(date -u '+%Y-%m-%dT%H:%M:%SZ')"
  echo "Build type: ${BUILD_TYPE}"
  echo
  echo "Executable:"
  file "${EXE}" || true
  echo
  echo "Runtime shared-library dependencies:"
  ldd "${EXE}" || true
} > "${PREFIX_DIR}/share/doc/openFPGALoader/docker-linux-build-manifest.txt"

VERSION="$("${EXE}" --version 2>/dev/null | head -n1 | tr -cs 'A-Za-z0-9._-' '-' | sed 's/^-//;s/-$//' || true)"

if [[ -z "${VERSION}" ]]; then
  VERSION="local"
fi

ARCH="$(uname -m)"
ARCHIVE="${PACKAGE_DIR}/openFPGALoader-linux-${ARCH}-${VERSION}.tar.gz"

rm -f "${ARCHIVE}" "${ARCHIVE}.sha256"

(
  cd "${PREFIX_DIR}"
  tar -czf "${ARCHIVE}" .
)

sha256sum "${ARCHIVE}" > "${ARCHIVE}.sha256"

echo "Built ${EXE}"
echo "Package: ${ARCHIVE}"
echo "Checksum: ${ARCHIVE}.sha256"
