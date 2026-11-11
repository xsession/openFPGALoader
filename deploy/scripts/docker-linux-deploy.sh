#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-docker-linux}"
PREFIX_DIR="${PREFIX_DIR:-${ROOT_DIR}/dist/docker-linux/install}"
PACKAGE_DIR="${PACKAGE_DIR:-${ROOT_DIR}/dist/docker-linux}"
BUILD_TYPE="${CMAKE_BUILD_TYPE:-Release}"
CMAKE_EXTRA_ARGS="${CMAKE_EXTRA_ARGS:-}"
# Labels the package per libc base: "linux" (Alpine/musl) by default,
# "arch-linux" for the Arch/glibc image.
ARCH_LABEL="${ARCH_LABEL:-linux}"

if [[ "${1:-}" == "--clean" ]]; then
  rm -rf "${BUILD_DIR}" "${PREFIX_DIR}"
  shift
fi

mkdir -p "${BUILD_DIR}" "${PREFIX_DIR}" "${PACKAGE_DIR}"

# Drop stale archives from earlier runs so the "exactly one package" check
# in CI (and locally) can't see leftovers from a previous version.
rm -f "${PACKAGE_DIR}"/openFPGALoader-"${ARCH_LABEL}-"*.tar.gz{,.sha256}

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

# Optional in-container smoke test (set SMOKE_TEST=1 from CI or locally)
if [[ -n "${SMOKE_TEST:-}" ]]; then
  echo "Smoke test (in container):"
  "${EXE}" --version
  "${EXE}" --help >/dev/null
  "${EXE}" --list-boards >/dev/null
  "${EXE}" --list-cables >/dev/null
fi

mkdir -p "${PREFIX_DIR}/share/openFPGALoader"

LOCAL_FIRMWARE_DIR="${ROOT_DIR}/deploy/ise_programmer_bins"

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
  echo "openFPGALoader ${ARCH_LABEL} Docker deployment"
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

# --version prints "openFPGALoader v1.1.7"; keep only the version token so
# the archive is openFPGALoader-<label>-<arch>-v1.1.7.tar.gz (no doubled name).
VERSION="$("${EXE}" --version 2>/dev/null | head -n1 | awk '{print $NF}' | tr -cs 'A-Za-z0-9._-' '-' | sed 's/^-//;s/-$//' || true)"

if [[ -z "${VERSION}" ]]; then
  VERSION="local"
fi

ARCH="$(uname -m)"
ARCHIVE="${PACKAGE_DIR}/openFPGALoader-${ARCH_LABEL}-${ARCH}-${VERSION}.tar.gz"

rm -f "${ARCHIVE}" "${ARCHIVE}.sha256"

(
  cd "${PREFIX_DIR}"
  tar -czf "${ARCHIVE}" .
)

# Record the basename so the checksum file verifies from any directory
# (CI checks it on the host, where the /src container paths do not exist).
(cd "${PACKAGE_DIR}" && sha256sum "$(basename "${ARCHIVE}")" > "${ARCHIVE}.sha256")

# Distro installer packages from the same staged install tree:
#   .deb         where dpkg-deb is available (Alpine image: static binary)
#   .pkg.tar.zst where makepkg/zstd is available (Arch image: glibc binary)
# Staging happens on the container's native filesystem (mktemp under /tmp),
# not on the repository bind mount: dpkg-deb validates the control directory
# mode (0755..0775) and mount permission emulation is not portable.
if command -v dpkg-deb >/dev/null 2>&1; then
  echo "Building Debian package (.deb) for Ubuntu/Debian"
  PKG_STAGING="$(mktemp -d /tmp/openfpgaloader-deb.XXXXXX)"
  INSTALL_DIR="${PREFIX_DIR}" OUTPUT_DIR="${PACKAGE_DIR}" BUILD_DIR="${PKG_STAGING}" \
    bash "${ROOT_DIR}/deploy/scripts/build-linux-deb.sh"
  rm -rf "${PKG_STAGING}"
fi

if command -v makepkg >/dev/null 2>&1 || command -v zstd >/dev/null 2>&1; then
  echo "Building Arch package (.pkg.tar.zst)"
  PKG_STAGING="$(mktemp -d /tmp/openfpgaloader-pkg.XXXXXX)"
  INSTALL_DIR="${PREFIX_DIR}" OUTPUT_DIR="${PACKAGE_DIR}" BUILD_DIR="${PKG_STAGING}" \
    bash "${ROOT_DIR}/deploy/scripts/build-linux-pkg.sh"
  rm -rf "${PKG_STAGING}"
fi

echo "Built ${EXE}"
echo "Package: ${ARCHIVE}"
echo "Checksum: ${ARCHIVE}.sha256"
