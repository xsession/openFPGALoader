#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PACKAGING_DIR="${ROOT_DIR}/deploy/packaging/linux/debian"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist/docker-linux}"
INSTALL_DIR="${INSTALL_DIR:-${DIST_DIR}/install}"
OUTPUT_DIR="${OUTPUT_DIR:-${DIST_DIR}}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build-linux-deb}"
PACKAGE_NAME="${PACKAGE_NAME:-openFPGALoader}"

source "${SCRIPT_DIR}/package-common.sh"

EXE="${INSTALL_DIR}/bin/openFPGALoader"
require_file "${EXE}" "openFPGALoader executable"
require_file "${PACKAGING_DIR}/control" "Debian control file"
require_file "${PACKAGING_DIR}/postinst" "Debian post-install script"
require_file "${PACKAGING_DIR}/postrm" "Debian post-removal script"

VERSION="${VERSION:-$(package_version_from_executable "${EXE}" "${ROOT_DIR}")}"
mkdir -p "${OUTPUT_DIR}"

# Create staging directory
rm -rf "${BUILD_DIR}"
mkdir -p \
  "${BUILD_DIR}/usr/bin" \
  "${BUILD_DIR}/usr/share/openFPGALoader" \
  "${BUILD_DIR}/usr/share/doc/openfpgaloader" \
  "${BUILD_DIR}/usr/lib/udev/rules.d" \
  "${BUILD_DIR}/DEBIAN"

# Copy binary
cp "${EXE}" "${BUILD_DIR}/usr/bin/"
chmod 755 "${BUILD_DIR}/usr/bin/openFPGALoader"

# Copy runtime transport and probe firmware files when present.
if [[ -d "${INSTALL_DIR}/share/openFPGALoader" ]]; then
  cp -a "${INSTALL_DIR}/share/openFPGALoader/." "${BUILD_DIR}/usr/share/openFPGALoader/"
fi

cp "${ROOT_DIR}/LICENSE" "${BUILD_DIR}/usr/share/doc/openfpgaloader/copyright"
chmod 644 "${BUILD_DIR}/usr/share/doc/openfpgaloader/copyright"

# Copy udev rules
if [[ -f "${ROOT_DIR}/70-openfpgaloader.rules" ]]; then
  cp "${ROOT_DIR}/70-openfpgaloader.rules" "${BUILD_DIR}/usr/lib/udev/rules.d/"
fi
if [[ -f "${ROOT_DIR}/99-openfpgaloader.rules" ]]; then
  cp "${ROOT_DIR}/99-openfpgaloader.rules" "${BUILD_DIR}/usr/lib/udev/rules.d/"
fi

# Copy Debian control files into dpkg's required DEBIAN directory.
cp -a "${PACKAGING_DIR}/." "${BUILD_DIR}/DEBIAN/"
chmod 755 "${BUILD_DIR}/DEBIAN/postinst"
chmod 755 "${BUILD_DIR}/DEBIAN/postrm"

# Update version in control file if different
sed -i "s/^Version:.*/Version: ${VERSION}/" "${BUILD_DIR}/DEBIAN/control"

# Build .deb package. PACKAGE_NAME includes the matrix name in CI so Ubuntu
# 22.04 and Ubuntu 24.04 artifacts cannot overwrite one another at release.
PACKAGE_FILE="${OUTPUT_DIR}/${PACKAGE_NAME}_${VERSION}_amd64.deb"
dpkg-deb --build --root-owner-group "${BUILD_DIR}" "${PACKAGE_FILE}"

# Generate checksum
sha256sum "${PACKAGE_FILE}" > "${PACKAGE_FILE}.sha256"

echo "Built Debian package: ${PACKAGE_FILE}"
echo "Package contents:"
dpkg-deb --contents "${PACKAGE_FILE}" | head -40

# Clean up
rm -rf "${BUILD_DIR}"
