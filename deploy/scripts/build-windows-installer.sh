#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DIST_DIR="${DIST_DIR:-${ROOT_DIR}/dist/docker-windows}"
INSTALL_DIR="${INSTALL_DIR:-${DIST_DIR}/install}"
OUTPUT_DIR="${OUTPUT_DIR:-${DIST_DIR}}"
XPCU_DRIVER_ARCHIVE="${XPCU_DRIVER_ARCHIVE:-${INSTALL_DIR}/share/openFPGALoader/drivers/xilinx-platform-cable-windows.zip}"
XPCU_DRIVER_CHECKSUM="${XPCU_DRIVER_CHECKSUM:-${XPCU_DRIVER_ARCHIVE}.sha256}"
INNOSETUP_IMAGE="${INNOSETUP_IMAGE:-amake/innosetup@sha256:e003376ba818547275fe10c95e2a29be0f2d12d45e9eb8f205b6672dc5685bb1}"

source "${SCRIPT_DIR}/package-common.sh"

EXE="${INSTALL_DIR}/bin/openFPGALoader.exe"
require_file "${EXE}" "openFPGALoader.exe"
require_file "${XPCU_DRIVER_ARCHIVE}" "embedded Xilinx Platform Cable Windows driver archive"
require_file "${XPCU_DRIVER_CHECKSUM}" "embedded Xilinx Platform Cable Windows driver checksum"
(
  cd "$(dirname "${XPCU_DRIVER_ARCHIVE}")"
  sha256sum -c "$(basename "${XPCU_DRIVER_CHECKSUM}")"
)

VERSION="${VERSION:-$(package_version_from_executable "${EXE}" "${ROOT_DIR}")}"
INSTALLER_INPUT_DIR="${DIST_DIR}/installer"
XPCU_DRIVER_DIR="${INSTALLER_INPUT_DIR}/xilinx-platform-cable-windows"

mkdir -p "${OUTPUT_DIR}"
rm -rf "${XPCU_DRIVER_DIR}"
mkdir -p "${INSTALLER_INPUT_DIR}"
unzip -q "${XPCU_DRIVER_ARCHIVE}" -d "${INSTALLER_INPUT_DIR}"
require_file "${XPCU_DRIVER_DIR}/install.ps1" "Xilinx Platform Cable Windows installer script"
require_file "${XPCU_DRIVER_DIR}/bin/x64/wdi-simple.exe" "x64 libwdi helper"

echo "Building Windows installer..."

# Run Inno Setup via Docker (Wine maps Docker volumes to Z: drive)
docker run --rm \
  -v "${ROOT_DIR}/deploy/packaging/windows:/script:ro" \
  -v "${DIST_DIR}:/dist" \
  -v "${ROOT_DIR}/LICENSE:/license:ro" \
  -e WINEDEBUG=-all \
  "${INNOSETUP_IMAGE}" \
  /DMyAppVersion="${VERSION}" /script/openFPGALoader.iss

# Generate checksum
OUTPUT="${OUTPUT_DIR}/openFPGALoader-${VERSION}-win64-setup.exe"
if [[ -f "${OUTPUT}" ]]; then
  sha256sum "${OUTPUT}" > "${OUTPUT}.sha256"
  echo "Built Windows installer: ${OUTPUT}"
else
  echo "ERROR: Installer not found"
  exit 1
fi
