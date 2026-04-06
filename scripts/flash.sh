#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
TARGET_NAME="${TARGET_NAME:-stm32_touchscreen_dino}"
OPENOCD_INTERFACE="${OPENOCD_INTERFACE:-interface/stlink.cfg}"
OPENOCD_TARGET="${OPENOCD_TARGET:-target/stm32f4x.cfg}"
FLASH_ADDRESS="${FLASH_ADDRESS:-0x08000000}"

find_firmware() {
  local candidates=(
    "${BUILD_DIR}/${TARGET_NAME}.elf"
    "${BUILD_DIR}/${TARGET_NAME}.bin"
    "${BUILD_DIR}/${TARGET_NAME}"
  )
  local candidate

  for candidate in "${candidates[@]}"; do
    if [[ -f "${candidate}" ]]; then
      printf '%s\n' "${candidate}"
      return 0
    fi
  done

  return 1
}

collect_stlink_devices() {
  local sysdev vendor_file vendor product_file product serial_file serial manufacturer_file manufacturer
  for vendor_file in /sys/bus/usb/devices/*/idVendor; do
    [[ -f "${vendor_file}" ]] || continue
    vendor="$(<"${vendor_file}")"
    [[ "${vendor}" == "0483" ]] || continue

    sysdev="$(dirname "${vendor_file}")"
    product_file="${sysdev}/product"
    manufacturer_file="${sysdev}/manufacturer"
    serial_file="${sysdev}/serial"

    product="$( [[ -f "${product_file}" ]] && <"${product_file}" || printf 'Unknown product' )"
    manufacturer="$( [[ -f "${manufacturer_file}" ]] && <"${manufacturer_file}" || printf 'Unknown vendor' )"
    serial="$( [[ -f "${serial_file}" ]] && <"${serial_file}" || printf '' )"

    if [[ "${manufacturer}" == *"STMicro"* || "${product}" == *"ST-LINK"* || -n "${serial}" ]]; then
      printf '%s|%s|%s\n' "${product}" "${serial}" "${sysdev##*/}"
    fi
  done
}

print_usb_diagnostics() {
  local sysdev vendor_file vendor product_file product manufacturer_file manufacturer serial_file serial

  echo "Visible USB devices:"
  for vendor_file in /sys/bus/usb/devices/*/idVendor; do
    [[ -f "${vendor_file}" ]] || continue
    sysdev="$(dirname "${vendor_file}")"
    product_file="${sysdev}/product"
    manufacturer_file="${sysdev}/manufacturer"
    serial_file="${sysdev}/serial"
    vendor="$(<"${vendor_file}")"
    product="$( [[ -f "${product_file}" ]] && <"${product_file}" || printf 'Unknown product' )"
    manufacturer="$( [[ -f "${manufacturer_file}" ]] && <"${manufacturer_file}" || printf 'Unknown vendor' )"
    serial="$( [[ -f "${serial_file}" ]] && <"${serial_file}" || printf '' )"

    if [[ -n "${serial}" ]]; then
      printf '  %s vendor=%s product=%s manufacturer="%s" name="%s" serial="%s"\n' \
        "${sysdev##*/}" "${vendor}" "$( <"${sysdev}/idProduct" )" "${manufacturer}" "${product}" "${serial}"
    else
      printf '  %s vendor=%s product=%s manufacturer="%s" name="%s"\n' \
        "${sysdev##*/}" "${vendor}" "$( <"${sysdev}/idProduct" )" "${manufacturer}" "${product}"
    fi
  done
}

if ! command -v openocd >/dev/null 2>&1; then
  echo "openocd is not installed or not in PATH" >&2
  exit 1
fi

if ! firmware_path="$(find_firmware)"; then
  echo "No firmware artifact found in ${BUILD_DIR}" >&2
  echo "Expected one of:" >&2
  echo "  ${BUILD_DIR}/${TARGET_NAME}.elf" >&2
  echo "  ${BUILD_DIR}/${TARGET_NAME}.bin" >&2
  echo "  ${BUILD_DIR}/${TARGET_NAME}" >&2
  exit 1
fi

mapfile -t devices < <(collect_stlink_devices)

if ((${#devices[@]} == 0)); then
  echo "No ST-LINK USB devices detected" >&2
  echo >&2
  print_usb_diagnostics >&2
  echo >&2
  echo "Expected an ST-LINK style USB device, usually vendor 0483." >&2
  echo "If your board is connected, make sure you are using the ST-LINK USB connector and not only the target USB/UART port." >&2
  exit 1
fi

echo "Select the ST-LINK device to use:"

for i in "${!devices[@]}"; do
  IFS='|' read -r product serial sysname <<<"${devices[i]}"
  if [[ -n "${serial}" ]]; then
    printf '  %d) %s [serial: %s] [%s]\n' "$((i + 1))" "${product}" "${serial}" "${sysname}"
  else
    printf '  %d) %s [%s]\n' "$((i + 1))" "${product}" "${sysname}"
  fi
done

read -r -p "Enter number: " selection

if ! [[ "${selection}" =~ ^[0-9]+$ ]] || ((selection < 1 || selection > ${#devices[@]})); then
  echo "Invalid selection" >&2
  exit 1
fi

IFS='|' read -r selected_product selected_serial selected_sysname <<<"${devices[selection - 1]}"

openocd_args=(
  -f "${OPENOCD_INTERFACE}"
  -f "${OPENOCD_TARGET}"
)

if [[ -n "${selected_serial}" ]]; then
  openocd_args+=(-c "adapter serial ${selected_serial}")
fi

if [[ "${firmware_path}" == *.bin ]]; then
  openocd_args+=(-c "program ${firmware_path} ${FLASH_ADDRESS} verify reset exit")
else
  openocd_args+=(-c "program ${firmware_path} verify reset exit")
fi

echo "Flashing ${firmware_path} to ${selected_product}"
openocd "${openocd_args[@]}"
