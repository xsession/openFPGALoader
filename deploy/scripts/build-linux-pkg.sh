#!/usr/bin/env bash
set -euo pipefail

# Build an Arch Linux package (.pkg.tar.zst) from a staged install tree,
# the Linux counterpart of build-linux-deb.sh.
#
# Primary path: construct the package format directly (.PKGINFO tarball,
# zstd-compressed, root-owned). This is deterministic and immune to
# makepkg CLI drift on a rolling distro (makepkg moved to libmakepkg and
# dropped flags such as --nopackager). makepkg with the committed PKGBUILD
# is kept as a fallback for hosts without zstd.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGING_DIR="${ROOT_DIR}/deploy/packaging/linux/arch"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist/docker-linux}"
INSTALL_DIR="${INSTALL_DIR:-${DIST_DIR}/install}"
OUTPUT_DIR="${OUTPUT_DIR:-${DIST_DIR}}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-linux-pkg}"
PACKAGE_NAME="openfpgaloader"
PKGREL="1"

source "${SCRIPT_DIR}/package-common.sh"

EXE="${INSTALL_DIR}/bin/openFPGALoader"
require_file "${EXE}" "openFPGALoader executable"
require_file "${PACKAGING_DIR}/PKGBUILD" "Arch PKGBUILD"

VERSION="${VERSION:-$(package_version_from_executable "${EXE}" "${ROOT_DIR}")}"
mkdir -p "${OUTPUT_DIR}"

# Create the staged root tree (same /usr layout as the .deb builder).
rm -rf "${BUILD_DIR}"
ROOT="${BUILD_DIR}/root"
mkdir -p \
  "${ROOT}/usr/bin" \
  "${ROOT}/usr/share/openFPGALoader" \
  "${ROOT}/usr/share/licenses/openfpgaloader" \
  "${ROOT}/usr/lib/udev/rules.d"

# Copy binary
install -m 755 "${EXE}" "${ROOT}/usr/bin/openFPGALoader"

# Copy runtime transport and probe firmware files when present.
if [[ -d "${INSTALL_DIR}/share/openFPGALoader" ]]; then
  cp -a "${INSTALL_DIR}/share/openFPGALoader/." "${ROOT}/usr/share/openFPGALoader/"
fi

# License (Arch convention: usr/share/licenses/<pkgname>/)
install -m 644 "${ROOT_DIR}/LICENSE" \
  "${ROOT}/usr/share/licenses/openfpgaloader/LICENSE"

# Copy udev rules
for rules in "${ROOT_DIR}/70-openfpgaloader.rules" "${ROOT_DIR}/99-openfpgaloader.rules"; do
  if [[ -f "${rules}" ]]; then
    install -m 644 "${rules}" "${ROOT}/usr/lib/udev/rules.d/"
  fi
done

PACKAGE_FILE="${OUTPUT_DIR}/${PACKAGE_NAME}-${VERSION}-${PKGREL}-x86_64.pkg.tar.zst"

if command -v zstd >/dev/null 2>&1; then
  # Primary: write .PKGINFO and build the tarball directly (the format
  # pacman consumes: root-owned tarball with a .PKGINFO metadata file).
  # The size field must equal the final compressed size, so iterate until
  # it stabilizes. Everything else is kept deterministic (fixed mtimes,
  # one frozen timestamp, single-threaded zstd) so the loop converges.
  TS="$(date +%s)"
  write_pkginfo() {
    {
      echo "pkgname=${PACKAGE_NAME}"
      echo "pkgver=${VERSION}-${PKGREL}"
      echo "pkgdesc=openFPGALoader: universal utility for programming FPGA"
      echo "arch=x86_64"
      echo "url=https://github.com/xsession/openFPGALoader"
      echo "license=Apache-2.0"
      echo "builddate=${TS}"
      echo "mtime=${TS}"
      echo "size=$1"
    } > "${ROOT}/.PKGINFO"
  }
  write_pkginfo 0
  CONVERGED=0
  for _ in 1 2 3 4 5 6 7 8; do
    (
      cd "${ROOT}"
      tar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 -cf - . \
        | zstd -q -f -19 -o "${PACKAGE_FILE}"
    )
    SIZE="$(stat -c%s "${PACKAGE_FILE}")"
    grep -q "^size=${SIZE}$" "${ROOT}/.PKGINFO" && { CONVERGED=1; break; }
    write_pkginfo "${SIZE}"
  done
  # Hard guard: never ship a package whose size field is wrong (pacman
  # validates it on install).
  DECLARED="$(tar -xOf "${PACKAGE_FILE}" ./.PKGINFO | sed -n 's/^size=//p')"
  ACTUAL="$(stat -c%s "${PACKAGE_FILE}")"
  if [[ "${CONVERGED}" != 1 || "${DECLARED}" != "${ACTUAL}" ]]; then
    echo "ERROR: Arch package size field did not converge (declared=${DECLARED} actual=${ACTUAL})" >&2
    exit 1
  fi
elif command -v makepkg >/dev/null 2>&1; then
  # Fallback: makepkg runs the committed PKGBUILD; the env vars hand it the
  # staged tree. Keep the flag set minimal (--force) because makepkg's CLI
  # drifts on a rolling distro.
  mkdir -p "${BUILD_DIR}/pkgbuild"
  cp "${PACKAGING_DIR}/PKGBUILD" "${BUILD_DIR}/pkgbuild/"
  (
    cd "${BUILD_DIR}/pkgbuild"
    OPENFPGALOADER_STAGE_DIR="${ROOT}" \
    OPENFPGALOADER_PKG_VER="${VERSION}" \
    OPENFPGALOADER_LICENSE="${ROOT_DIR}/LICENSE" \
    makepkg -f
  )
  BUILT="$(find "${BUILD_DIR}/pkgbuild" -maxdepth 1 \
    -name "${PACKAGE_NAME}-${VERSION}-${PKGREL}-x86_64.pkg.tar.*" | head -n 1)"
  if [[ -z "${BUILT}" ]]; then
    echo "ERROR: makepkg produced no package" >&2
    exit 1
  fi
  mv "${BUILT}" "${PACKAGE_FILE}"
else
  echo "ERROR: neither zstd nor makepkg is available to build the Arch package" >&2
  exit 1
fi

# Generate checksum (basename so it verifies from any directory)
chmod 644 "${PACKAGE_FILE}"
(cd "${OUTPUT_DIR}" && sha256sum "$(basename "${PACKAGE_FILE}")" > "${PACKAGE_FILE}.sha256")
chmod 644 "${PACKAGE_FILE}.sha256"

echo "Built Arch package: ${PACKAGE_FILE}"
echo "Package contents:"
tar -tf "${PACKAGE_FILE}" | sed -n '1,40p'

# Clean up
rm -rf "${BUILD_DIR}"
