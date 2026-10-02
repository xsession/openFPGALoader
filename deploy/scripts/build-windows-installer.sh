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
ALPINE_IMAGE="${ALPINE_IMAGE:-alpine:3.24.2}"

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
OUTPUT_BASENAME="openFPGALoader-${VERSION}-win64-setup.exe"
OUTPUT="${OUTPUT_DIR}/${OUTPUT_BASENAME}"

mkdir -p "${OUTPUT_DIR}"
rm -rf "${XPCU_DRIVER_DIR}"
mkdir -p "${INSTALLER_INPUT_DIR}"
unzip -q "${XPCU_DRIVER_ARCHIVE}" -d "${INSTALLER_INPUT_DIR}"
require_file "${XPCU_DRIVER_DIR}/install.ps1" "Xilinx Platform Cable Windows installer script"
require_file "${XPCU_DRIVER_DIR}/bin/x64/wdi-simple.exe" "x64 libwdi helper"
require_file "${XPCU_DRIVER_DIR}/bin/x86/wdi-simple.exe" "x86 libwdi helper"

echo "Building Windows installer..."

# amake/innosetup deliberately runs Wine/Inno Setup as an unprivileged xclient
# user. On GitHub-hosted runners and some Docker Desktop setups, that user can
# read a bind-mounted workspace but cannot create Inno's temporary .e32.tmp
# files there. Compile into a Docker-managed volume instead, then copy only the
# finished installer back to the host output directory.
INNO_OUTPUT_VOLUME="${INNO_OUTPUT_VOLUME:-openfpgaloader-innosetup-${GITHUB_RUN_ID:-local}-${GITHUB_RUN_ATTEMPT:-0}-$$}"
cleanup_inno_volume() {
  docker volume rm -f "${INNO_OUTPUT_VOLUME}" >/dev/null 2>&1 || true
}
trap cleanup_inno_volume EXIT INT TERM

docker volume create "${INNO_OUTPUT_VOLUME}" >/dev/null
# Named volumes start root-owned. Make the dedicated output volume writable by
# the non-root xclient user inside the Inno Setup image.
docker run --rm \
  -v "${INNO_OUTPUT_VOLUME}:/out" \
  "${ALPINE_IMAGE}" \
  sh -eu -c 'chmod 0777 /out'

# Inputs remain read-only. Wine maps the Linux container root to Z:, therefore
# /out is available to Inno Setup as Z:\out.
docker run --rm \
  -v "${ROOT_DIR}/deploy/packaging/windows:/script:ro" \
  -v "${DIST_DIR}:/dist:ro" \
  -v "${ROOT_DIR}/LICENSE:/license:ro" \
  -v "${INNO_OUTPUT_VOLUME}:/out" \
  -e WINEDEBUG=-all \
  "${INNOSETUP_IMAGE}" \
  /DMyAppVersion="${VERSION}" \
  '/DMyOutputDir=Z:\out' \
  /script/openFPGALoader.iss

# Copy the completed installer out with a root Alpine helper. This keeps all
# Inno/Wine temporary writes off the host bind mount while preserving the
# existing dist/ artifact layout expected by CI and release publishing.
rm -f "${OUTPUT}" "${OUTPUT}.sha256"
docker run --rm \
  -v "${INNO_OUTPUT_VOLUME}:/out:ro" \
  -v "${OUTPUT_DIR}:/host-out" \
  -e OUTPUT_BASENAME="${OUTPUT_BASENAME}" \
  "${ALPINE_IMAGE}" \
  sh -eu -c '
    test -s "/out/${OUTPUT_BASENAME}"
    cp "/out/${OUTPUT_BASENAME}" "/host-out/${OUTPUT_BASENAME}"
  '

if [[ -f "${OUTPUT}" ]]; then
  sha256sum "${OUTPUT}" > "${OUTPUT}.sha256"
  echo "Built Windows installer: ${OUTPUT}"
else
  echo "ERROR: Installer not found after Docker volume copy-back: ${OUTPUT}" >&2
  exit 1
fi
