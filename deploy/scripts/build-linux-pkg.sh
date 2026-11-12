#!/usr/bin/env bash
set -euo pipefail

# Build an Arch Linux package (.pkg.tar.zst) from a staged install tree,
# the Linux counterpart of build-linux-deb.sh.
#
# Primary path: makepkg with the committed PKGBUILD (makepkg ships in the
# pacman package, so it is present in the Arch deploy image). It is the
# reference packager, so the .PKGINFO metadata — including the
# self-referential `size` field — is handled correctly by definition.
#
# Fallback (hosts without makepkg, e.g. local dev on Debian/Ubuntu):
# construct the package format directly (.PKGINFO tarball, zstd-compressed,
# root-owned) with a best-effort size field.

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

if command -v makepkg >/dev/null 2>&1; then
  # Primary: makepkg (ships in the pacman package, present in the Arch
  # deploy image). It is the reference packager: the .PKGINFO, .BUILDINFO,
  # ownership and the self-referential `size` field are all its job, and
  # its output is exactly what pacman installs on Arch.
  #
  # Use the plain `makepkg` invocation (no extra flags) to keep the CLI
  # surface minimal on a rolling distro, in a fresh directory so no
  # "package already exists" handling is needed.
  rm -rf "${BUILD_DIR}/pkgbuild"
  mkdir -p "${BUILD_DIR}/pkgbuild"
  cp "${PACKAGING_DIR}/PKGBUILD" "${BUILD_DIR}/pkgbuild/"
  (
    cd "${BUILD_DIR}/pkgbuild"
    OPENFPGALOADER_STAGE_DIR="${ROOT}" \
    OPENFPGALOADER_PKG_VER="${VERSION}" \
    OPENFPGALOADER_LICENSE="${ROOT_DIR}/LICENSE" \
    makepkg < /dev/null
  )
  BUILT="$(find "${BUILD_DIR}/pkgbuild" -maxdepth 1 \
    -name "${PACKAGE_NAME}-${VERSION}-${PKGREL}-x86_64.pkg.tar.*" | head -n 1)"
  if [[ -z "${BUILT}" ]]; then
    echo "ERROR: makepkg produced no package" >&2
    exit 1
  fi
  mv "${BUILT}" "${PACKAGE_FILE}"
elif command -v zstd >/dev/null 2>&1; then
  # Fallback (hosts without makepkg, e.g. local dev on Debian/Ubuntu):
  # write .PKGINFO and build the tarball directly.
  #
  # The `size` field is self-referential: its digits are bytes inside the
  # file whose compressed length they determine, so an exact fixed point
  # does not always exist (the compressed size can 2-cycle by a couple of
  # bytes). pacman treats the field as informational, so build
  # deterministically (fixed mtimes, one frozen timestamp, single-threaded
  # zstd), iterate until stable, and accept a small residual difference.
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
  for _ in 1 2 3 4 5 6 7 8; do
    (
      cd "${ROOT}"
      tar --sort=name --owner=0 --group=0 --numeric-owner --mtime=@0 -cf - . \
        | zstd -q -f -19 -o "${PACKAGE_FILE}"
    )
    SIZE="$(stat -c%s "${PACKAGE_FILE}")"
    grep -q "^size=${SIZE}$" "${ROOT}/.PKGINFO" && break
    write_pkginfo "${SIZE}"
  done
  # Sanity guard: the size field must be present and within a few bytes of
  # the real file size (a large delta means a real packaging bug).
  DECLARED="$(tar -xOf "${PACKAGE_FILE}" ./.PKGINFO | sed -n 's/^size=//p')"
  ACTUAL="$(stat -c%s "${PACKAGE_FILE}")"
  DELTA=$(( ACTUAL > DECLARED ? ACTUAL - DECLARED : DECLARED - ACTUAL ))
  if [[ -z "${DECLARED}" || "${DELTA}" -gt 64 ]]; then
    echo "ERROR: Arch package size field invalid (declared=${DECLARED:-<missing>} actual=${ACTUAL})" >&2
    exit 1
  fi
else
  echo "ERROR: neither makepkg nor zstd is available to build the Arch package" >&2
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
