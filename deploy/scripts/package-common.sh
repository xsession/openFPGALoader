#!/usr/bin/env bash

# Shared helpers for platform package scripts.
# This file is sourced by build-linux-deb.sh and build-windows-installer.sh.

set -euo pipefail

package_version_from_executable() {
  local executable="$1"
  local source_root="${2:-}"
  local raw=""
  local version=""

  if [[ -n "${OPENFPGALOADER_PACKAGE_VERSION:-}" ]]; then
    version="${OPENFPGALOADER_PACKAGE_VERSION#v}"
  elif [[ -x "${executable}" ]]; then
    raw="$("${executable}" --version 2>/dev/null | head -n 1 || true)"
    if [[ "${raw}" =~ ([0-9]+(\.[0-9]+){1,3}) ]]; then
      version="${BASH_REMATCH[1]}"
    fi
  fi

  # A Windows executable cannot be executed by the Linux packaging runner.
  # Resolve the same version source used by CMake before falling back to the
  # source-archive version embedded in CMakeLists.txt.
  if [[ -z "${version}" && -n "${source_root}" && -d "${source_root}/.git" ]] && command -v git >/dev/null 2>&1; then
    raw="$(git -C "${source_root}" describe --tags --match 'v[0-9]*' --abbrev=0 2>/dev/null || true)"
    if [[ "${raw}" =~ ^v([0-9]+(\.[0-9]+){1,3})$ ]]; then
      version="${BASH_REMATCH[1]}"
    fi
  fi

  if [[ -z "${version}" && -n "${source_root}" && -f "${source_root}/CMakeLists.txt" ]]; then
    version="$(sed -n 's/^set(_openfpgaloader_fallback_version "\([0-9][0-9.]*\)").*/\1/p' "${source_root}/CMakeLists.txt" | head -n 1)"
  fi

  if [[ -z "${version}" ]]; then
    version="0.0.0"
  fi

  if [[ ! "${version}" =~ ^[0-9]+(\.[0-9]+){1,3}$ ]]; then
    echo "ERROR: invalid package version '${version}'" >&2
    echo "       Set OPENFPGALOADER_PACKAGE_VERSION to a numeric version such as 1.1.4." >&2
    return 1
  fi

  printf '%s\n' "${version}"
}

require_file() {
  local path="$1"
  local description="$2"

  if [[ ! -f "${path}" ]]; then
    echo "ERROR: ${description} not found: ${path}" >&2
    return 1
  fi
}
