#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
TARGET_NAME="${TARGET_NAME:-stm32_touchscreen_dino}"
TOOLCHAIN_FILE_DEFAULT="${ROOT_DIR}/cmake/arm-gcc-toolchain.cmake"

cmake_args=(
  -S "${ROOT_DIR}"
  -B "${BUILD_DIR}"
  -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
)

if [[ -n "${CMAKE_TOOLCHAIN_FILE:-}" ]]; then
  cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}")
elif [[ -f "${TOOLCHAIN_FILE_DEFAULT}" ]]; then
  cmake_args+=("-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN_FILE_DEFAULT}")
fi

if (($# > 0)); then
  cmake_args+=("$@")
fi

echo "Configuring project in ${BUILD_DIR}"
cmake "${cmake_args[@]}"

echo "Building project"
cmake --build "${BUILD_DIR}" --parallel

artifact_base="${BUILD_DIR}/${TARGET_NAME}"
if [[ -f "${artifact_base}" ]] && command -v arm-none-eabi-objcopy >/dev/null 2>&1; then
  echo "Generating ${artifact_base}.bin"
  arm-none-eabi-objcopy -O binary "${artifact_base}" "${artifact_base}.bin"
fi

echo "Build complete"
