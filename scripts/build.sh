#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ROOT_NAME="$(basename "${ROOT_DIR}")"
PARENT_DIR="$(dirname "${ROOT_DIR}")"
BOARD="${BOARD:-dino_nucleo_f429zi}"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build/${BOARD}}"
LOCAL_BOARD_ROOT="${ROOT_DIR}"
LOCAL_ZEPHYR_BASE="${ROOT_DIR}/zephyr"
LOCAL_EXTERNAL_ZEPHYR_BASE="${ROOT_DIR}/external/zephyr"
LOCAL_ZEPHYR_SDK="${ROOT_DIR}/toolchain/zephyr-sdk"
SHARED_ZEPHYR_SDK="${PARENT_DIR}/zephyr-sdk"
SIBLING_PROJECT_ZEPHYR_SDK="${PARENT_DIR}/stm32-touchscreen-dino/toolchain/zephyr-sdk"
SDK_INSTALL_DIR_DEFAULT="${ZEPHYR_SDK_INSTALL_DIR:-${SHARED_ZEPHYR_SDK}}"
WEST_TOPDIR="${WEST_TOPDIR:-}"
PRISTINE="${PRISTINE:-auto}"

detect_zephyr_sdk() {
  local candidate
  for candidate in \
    "${ZEPHYR_SDK_INSTALL_DIR:-}" \
    "${LOCAL_ZEPHYR_SDK}" \
    "${SIBLING_PROJECT_ZEPHYR_SDK}" \
    "${SHARED_ZEPHYR_SDK}" \
    "${HOME}/zephyr-sdk" \
    "${HOME}"/zephyr-sdk-* \
    "/opt/zephyr-sdk" \
    /opt/zephyr-sdk-* \
    "/usr/local/zephyr-sdk" \
    /usr/local/zephyr-sdk-*; do
    [[ -n "${candidate}" ]] || continue
    if [[ -f "${candidate}/sdk_version" && \
          -f "${candidate}/cmake/zephyr/host-tools.cmake" && \
          ( -f "${candidate}/cmake/zephyr/gnu/generic.cmake" || \
            -f "${candidate}/cmake/zephyr/llvm/generic.cmake" ) ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  return 1
}

detect_gnuarmemb() {
  local candidate
  for candidate in \
    "${GNUARMEMB_TOOLCHAIN_PATH:-}" \
    "${HOME}/gcc-arm-none-eabi" \
    "${HOME}"/gcc-arm-none-eabi-* \
    "${HOME}/arm-gnu-toolchain" \
    "${HOME}"/arm-gnu-toolchain-* \
    "/opt/gcc-arm-none-eabi" \
    /opt/gcc-arm-none-eabi-* \
    "/opt/arm-gnu-toolchain" \
    /opt/arm-gnu-toolchain-* \
    "/usr/local/gcc-arm-none-eabi" \
    /usr/local/gcc-arm-none-eabi-*; do
    [[ -n "${candidate}" ]] || continue
    if [[ -x "${candidate}/bin/arm-none-eabi-gcc" || -x "${candidate}/bin/arm-none-eabi-g++" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done
  return 1
}

has_west_build_command() {
  if [[ ! -f "${ZEPHYR_BASE}/scripts/west_commands/build.py" ]]; then
    return 1
  fi

  west help 2>/dev/null | grep -q "build:"
}

cached_zephyr_base() {
  local cache_file="${BUILD_DIR}/CMakeCache.txt"

  if [[ ! -f "${cache_file}" ]]; then
    return 1
  fi

  sed -n 's/^ZEPHYR_BASE:PATH=//p' "${cache_file}" | head -n 1
}

ensure_west_workspace() {
  if [[ -n "${WEST_TOPDIR}" ]]; then
    return 0
  fi

  echo "Initializing west workspace in ${PARENT_DIR}"
  (
    cd "${PARENT_DIR}"
    west init -l "${ROOT_NAME}"
  )

  WEST_TOPDIR="${PARENT_DIR}"
}

resolve_zephyr_base_from_west() {
  local zephyr_base_rel

  zephyr_base_rel="$(west config zephyr.base 2>/dev/null || true)"
  if [[ -n "${zephyr_base_rel}" && -d "${WEST_TOPDIR}/${zephyr_base_rel}" ]]; then
    printf '%s\n' "${WEST_TOPDIR}/${zephyr_base_rel}"
    return 0
  fi

  return 1
}

ensure_zephyr_sdk() {
  if sdk_dir="$(detect_zephyr_sdk)"; then
    export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
    export ZEPHYR_SDK_INSTALL_DIR="${sdk_dir}"
    echo "Using Zephyr SDK at ${ZEPHYR_SDK_INSTALL_DIR}"
    return 0
  fi

  echo "Installing Zephyr SDK into ${SDK_INSTALL_DIR_DEFAULT}"
  (
    cd "${ROOT_DIR}"
    west sdk install --install-dir "${SDK_INSTALL_DIR_DEFAULT}" --gnu-toolchains arm-zephyr-eabi
  )

  sdk_dir="$(detect_zephyr_sdk)" || {
    echo "Zephyr SDK installation finished, but no valid SDK was detected." >&2
    exit 1
  }

  export ZEPHYR_TOOLCHAIN_VARIANT=zephyr
  export ZEPHYR_SDK_INSTALL_DIR="${sdk_dir}"
  echo "Using Zephyr SDK at ${ZEPHYR_SDK_INSTALL_DIR}"
}

if ! command -v west >/dev/null 2>&1; then
  echo "west is not installed or not in PATH" >&2
  exit 1
fi

if [[ -z "${WEST_TOPDIR}" ]]; then
  WEST_TOPDIR="$(west topdir 2>/dev/null || true)"
fi

FIRST_BUILD=0
if [[ ! -f "${BUILD_DIR}/build.ninja" ]]; then
  FIRST_BUILD=1
fi

if [[ -z "${WEST_TOPDIR}" ]]; then
  ensure_west_workspace
  FIRST_BUILD=1
fi

zephyr_base_from_west="$(resolve_zephyr_base_from_west || true)"

if [[ -n "${zephyr_base_from_west}" ]]; then
  workspace_zephyr_dir="${zephyr_base_from_west}"
else
  workspace_zephyr_dir="${WEST_TOPDIR}/zephyr"
fi

if [[ ! -d "${workspace_zephyr_dir}" ]]; then
  echo "Fetching Zephyr workspace sources"
  (
    cd "${WEST_TOPDIR}"
    west update
  )
  FIRST_BUILD=1
fi

if [[ -z "${ZEPHYR_BASE:-}" ]]; then
  if [[ -d "${LOCAL_EXTERNAL_ZEPHYR_BASE}" ]]; then
    export ZEPHYR_BASE="${LOCAL_EXTERNAL_ZEPHYR_BASE}"
  elif [[ -d "${LOCAL_ZEPHYR_BASE}" ]]; then
    export ZEPHYR_BASE="${LOCAL_ZEPHYR_BASE}"
  elif [[ -n "${zephyr_base_from_west}" ]]; then
    export ZEPHYR_BASE="${zephyr_base_from_west}"
  elif [[ -n "${WEST_TOPDIR}" && -d "${WEST_TOPDIR}/zephyr" ]]; then
    export ZEPHYR_BASE="${WEST_TOPDIR}/zephyr"
  else
    echo "ZEPHYR_BASE is not set and ${LOCAL_ZEPHYR_BASE} was not found" >&2
    echo "Run 'west init -l .' and 'west update' in the project root first." >&2
    exit 1
  fi
fi

if ! has_west_build_command; then
  echo "Completing Zephyr workspace fetch"
  (
    cd "${WEST_TOPDIR}"
    west update
  )
fi

cached_base="$(cached_zephyr_base || true)"
if [[ -n "${cached_base}" && "${cached_base}" != "${ZEPHYR_BASE}" ]]; then
  echo "Detected stale build directory configured for ${cached_base}"
  echo "Removing ${BUILD_DIR} and reconfiguring for ${ZEPHYR_BASE}"
  rm -rf "${BUILD_DIR}"
  FIRST_BUILD=1
fi

if [[ -z "${ZEPHYR_TOOLCHAIN_VARIANT:-}" ]]; then
  if gnuarmemb_dir="$(detect_gnuarmemb)"; then
    export ZEPHYR_TOOLCHAIN_VARIANT=gnuarmemb
    export GNUARMEMB_TOOLCHAIN_PATH="${gnuarmemb_dir}"
    echo "Using GNU Arm Embedded toolchain at ${GNUARMEMB_TOOLCHAIN_PATH}"
  else
    ensure_zephyr_sdk
  fi
fi

if [[ "${PRISTINE}" == "auto" ]]; then
  if ((FIRST_BUILD)); then
    PRISTINE="always"
  else
    PRISTINE="never"
  fi
fi

west_args=(
  build
  -p "${PRISTINE}"
  -b "${BOARD}"
  "${ROOT_DIR}"
  -d "${BUILD_DIR}"
  --
  "-DBOARD_ROOT=${LOCAL_BOARD_ROOT}"
)

if (($# > 0)); then
  west_args=(
    build
    -p "${PRISTINE}"
    -b "${BOARD}"
    "${ROOT_DIR}"
    -d "${BUILD_DIR}"
  )
  west_args+=("$@")
  west_args+=("--" "-DBOARD_ROOT=${LOCAL_BOARD_ROOT}")
fi

echo "Building Zephyr app for board ${BOARD} using board files from ${LOCAL_BOARD_ROOT}/boards"
west "${west_args[@]}"

echo "Build complete: ${BUILD_DIR}/zephyr/zephyr.elf"
