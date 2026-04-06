# stm32-touchscreen-dino

Minimal C++ project skeleton using CMake.

## Structure

```text
.
├── CMakeLists.txt
├── include
├── src
│   └── main.cpp
└── tests
    └── CMakeLists.txt
```

## Build

```bash
cmake -S . -B build
cmake --build build
```

Or use the helper script:

```bash
./scripts/build.sh
```

## Run

```bash
./build/stm32_touchscreen_dino
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Flash

If you have an STM32 firmware image and `openocd` installed:

```bash
./scripts/flash.sh
```

The script will detect connected ST-LINK USB devices, let you choose one, and then program the selected board.
