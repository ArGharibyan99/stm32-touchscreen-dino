# stm32-touchscreen-dino

Single-file Zephyr C++ application for the STM32 Nucleo-F429ZI board.

## Structure

```text
.
├── CMakeLists.txt
├── prj.conf
├── scripts
│   ├── build.sh
│   └── flash.sh
└── src
    └── main.cpp
```

## Board

This project is configured for the Zephyr board target `nucleo_f429zi`.

## Build

```bash
./scripts/build.sh
```

On a fresh checkout, the script will initialize the west workspace, fetch Zephyr sources if needed, and perform a pristine build.

On later builds, it skips those first-build setup steps and uses a non-pristine rebuild by default.

The effective build command is:

```bash
west build -p <always|never> -b nucleo_f429zi . -d build/nucleo_f429zi
```

## Docker Build

```bash
./scripts/docker-build.sh
```

This script only builds the local Docker image.

## Docker Run

```bash
./scripts/docker-run.sh
```

This starts an interactive container shell with the project mounted at the same absolute path as on the host. From inside the container you can run:

```bash
./scripts/build.sh
```

## Flash

```bash
./scripts/flash.sh
```

The flash script looks for connected ST-LINK probes, lets you choose one, and flashes the Zephyr output from `build/nucleo_f429zi/zephyr/`.

## Notes

- `src/main.cpp` is the Zephyr application entry point.
- `prj.conf` enables C++ support and console output.
- `west.yml` tracks Zephyr and its dependent repositories as external workspace sources.
- You need `west`, a working Zephyr environment, and `openocd`.
