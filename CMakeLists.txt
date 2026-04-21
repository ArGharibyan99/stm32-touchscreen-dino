cmake_minimum_required(VERSION 3.20.0)

list(APPEND BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/modules/fs/fatfs)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stm32_touchscreen_dino)

target_sources(app PRIVATE src/main.cpp src/display/display.cpp)
target_include_directories(app PRIVATE src)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external/zephyr/modules/fatfs)
target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/modules/fs/fatfs/include)
target_compile_definitions(app PRIVATE ZEPHYR_CONFIG_OVERRIDE=zephyr_fatfs_config.h)
