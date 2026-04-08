#!/usr/bin/env bash
set -euo pipefail

IMAGE_NAME="${IMAGE_NAME:-zephyr-stm32-env}"
PROJECT_DIR="${PROJECT_DIR:-$PWD}"
PROJECT_PARENT_DIR="$(dirname "${PROJECT_DIR}")"
CONTAINER_USER="${CONTAINER_USER:-host}"

if [[ ! -d "${PROJECT_DIR}" ]]; then
    echo "Error: PROJECT_DIR does not exist: ${PROJECT_DIR}" >&2
    exit 1
fi

DOCKER_ARGS=(
    --rm
    -it
    --privileged
    -v "${PROJECT_PARENT_DIR}:${PROJECT_PARENT_DIR}"
    -w "${PROJECT_DIR}"
)

if [[ -d /dev/bus/usb ]]; then
    DOCKER_ARGS+=( -v /dev/bus/usb:/dev/bus/usb )
fi

if [[ "${CONTAINER_USER}" == "root" ]]; then
    DOCKER_ARGS+=( --user root )
fi

if [[ -e /dev/ttyACM0 ]]; then
    DOCKER_ARGS+=(--device=/dev/ttyACM0)
fi

if [[ -e /dev/ttyUSB0 ]]; then
    DOCKER_ARGS+=(--device=/dev/ttyUSB0)
fi

docker run "${DOCKER_ARGS[@]}" "${IMAGE_NAME}"