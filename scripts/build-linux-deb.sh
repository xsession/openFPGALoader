#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PACKAGING_DIR="${ROOT_DIR}/packaging/linux"
DIST_DIR="${ROOT_DIR}/dist/docker-linux"
INSTALL_DIR="${DIST_DIR}/install"
BUILD_DIR="${ROOT_DIR}/build-linux-deb"

VERSION="1.1.2"

# Check prerequisites
if [[ ! -f "${INSTALL_DIR}/bin/openFPGALoader" ]]; then
  echo "ERROR: openFPGALoader binary not found at ${INSTALL_DIR}/bin/"
  echo "Build first: docker compose -f docker-compose.deploy-linux.yml run --rm linux-deploy"
  exit 1
fi

# Create staging directory
rm -rf "${BUILD_DIR}"
mkdir -p "${BUILD_DIR}/usr/bin"
mkdir -p "${BUILD_DIR}/usr/share/openFPGALoader"
mkdir -p "${BUILD_DIR}/usr/lib/udev/rules.d"

# Copy binary
cp "${INSTALL_DIR}/bin/openFPGALoader" "${BUILD_DIR}/usr/bin/"
chmod 755 "${BUILD_DIR}/usr/bin/openFPGALoader"

# Copy firmware files
cp -r "${INSTALL_DIR}/share/openFPGALoader/"* "${BUILD_DIR}/usr/share/openFPGALoader/"

# Copy udev rules
if [[ -f "${ROOT_DIR}/70-openfpgaloader.rules" ]]; then
  cp "${ROOT_DIR}/70-openfpgaloader.rules" "${BUILD_DIR}/usr/lib/udev/rules.d/"
fi
if [[ -f "${ROOT_DIR}/99-openfpgaloader.rules" ]]; then
  cp "${ROOT_DIR}/99-openfpgaloader.rules" "${BUILD_DIR}/usr/lib/udev/rules.d/"
fi

# Copy DEBIAN control files
cp -r "${PACKAGING_DIR}/DEBIAN" "${BUILD_DIR}/"
chmod 755 "${BUILD_DIR}/DEBIAN/postinst"
chmod 755 "${BUILD_DIR}/DEBIAN/postrm"

# Update version in control file if different
sed -i "s/^Version:.*/Version: ${VERSION}/" "${BUILD_DIR}/DEBIAN/control"

# Build .deb package
dpkg-deb --build --root-owner-group "${BUILD_DIR}" "${DIST_DIR}/openFPGALoader_${VERSION}_amd64.deb"

# Generate checksum
sha256sum "${DIST_DIR}/openFPGALoader_${VERSION}_amd64.deb" > "${DIST_DIR}/openFPGALoader_${VERSION}_amd64.deb.sha256"

echo "Built Debian package: ${DIST_DIR}/openFPGALoader_${VERSION}_amd64.deb"
echo "Package contents:"
dpkg-deb --contents "${DIST_DIR}/openFPGALoader_${VERSION}_amd64.deb" | head -20

# Clean up
rm -rf "${BUILD_DIR}"