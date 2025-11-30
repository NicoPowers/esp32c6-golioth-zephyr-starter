# SPDX-License-Identifier: MIT

set(BOARD esp32c6_devkitc/esp32c6/hpcore)
set(mcuboot_DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/mcuboot.overlay CACHE STRING "" FORCE)

find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sysbuild LANGUAGES)
