#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIST_DIR="${ROOT_DIR}/dist/docker-windows"
VERSION="1.1.2"

if [[ ! -f "${DIST_DIR}/install/bin/openFPGALoader.exe" ]]; then
  echo "ERROR: openFPGALoader.exe not found"
  echo "Build first: docker compose -f docker-compose.cross-windows.yml run --rm windows-cross"
  exit 1
fi

echo "Building Windows installer..."

# Run Inno Setup via Docker (Wine maps Docker volumes to Z: drive)
docker run --rm \
  -v "${ROOT_DIR}/packaging/windows:/script" \
  -v "${DIST_DIR}:/dist" \
  -v "${ROOT_DIR}/externals:/externals" \
  -v "${ROOT_DIR}/LICENSE:/license" \
  -e WINEDEBUG=-all \
  amake/innosetup:latest \
  /script/openFPGALoader.iss

# Generate checksum
OUTPUT="${DIST_DIR}/openFPGALoader-${VERSION}-win64-setup.exe"
if [[ -f "${OUTPUT}" ]]; then
  sha256sum "${OUTPUT}" > "${OUTPUT}.sha256"
  echo "Built Windows installer: ${OUTPUT}"
else
  echo "ERROR: Installer not found"
  exit 1
fi