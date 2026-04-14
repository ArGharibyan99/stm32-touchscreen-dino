# stm32-touchscreen-dino

Single-file Zephyr C++ application for the STM32 Nucleo-F429ZI board.

## Structure

```text
.
├── boards
│   └── st
│       └── dino_nucleo_f429zi
├── CMakeLists.txt
├── prj.conf
├── scripts
│   ├── build.sh
│   └── flash.sh
└── src
    └── main.cpp
```

## Board

This project is configured for the Zephyr board target `dino_nucleo_f429zi`.

## Build

```bash
./scripts/build.sh
```

On a fresh checkout, the script will initialize the west workspace, fetch Zephyr sources if needed, and perform a pristine build.

On later builds, it skips those first-build setup steps and uses a non-pristine rebuild by default.

The board definition for `dino_nucleo_f429zi` is copied into this repo under `boards/st/dino_nucleo_f429zi`, and the build passes `BOARD_ROOT` so DTS/Kconfig/board files come from your project instead of editing Zephyr's copy.

The effective build command is:

```bash
west build -p <always|never> -b dino_nucleo_f429zi . -d build/dino_nucleo_f429zi -- -DBOARD_ROOT=$PWD
```

## Animation Assets

`./scripts/build.sh` also converts the PNG animation frames from `animation/` into `.rgb565` files under `sdcard/animation/`.

To play the animation on the board, copy all generated `.rgb565` files from `sdcard/animation/` onto the SD card in the same `animation/` folder:

```text
SD card
└── animation
    ├── dino_frame_01.rgb565
    ├── dino_frame_02.rgb565
    ├── ...
    └── dino_frame_15.rgb565
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

The flash script looks for connected ST-LINK probes, lets you choose one, and flashes the Zephyr output from `build/dino_nucleo_f429zi/zephyr/`.

## Notes

- `src/main.cpp` is the Zephyr application entry point.
- `prj.conf` enables C++ support and console output.
- `west.yml` tracks Zephyr and its dependent repositories as external workspace sources.
- You need `west`, a working Zephyr environment, and `openocd`.
