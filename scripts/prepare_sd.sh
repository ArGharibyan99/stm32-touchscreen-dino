#!/usr/bin/env bash

set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

echo "Preparing SD card animation assets"
python3 "${ROOT_DIR}/tools/convert_animation_to_rgb565.py"

echo "SD assets ready under ${ROOT_DIR}/sdcard"
