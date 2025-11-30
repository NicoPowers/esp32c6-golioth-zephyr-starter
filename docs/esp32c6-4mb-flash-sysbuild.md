# Configuring ESP32-C6 with 4MB Flash for Zephyr Sysbuild + MCUboot

## Problem

The ESP32-C6 DevKitC board definition in Zephyr defaults to an 8MB flash module (`esp32c6_wroom_n8.dtsi`), but our hardware uses a 4MB flash chip. When using sysbuild with MCUboot, the boot failed with:

```
I (40) flash_init: SPI Flash Size : 8MB
...
E (1880) spi_flash: Detected size(4096k) smaller than the size in the binary image header(8192k). Probe failed.
E (1887) flash_init: Failed to init flash chip: 260
```

## Root Causes

There were three separate issues to address:

### 1. Board Selection with Sysbuild

Setting `BOARD` in the app's `CMakeLists.txt` doesn't work with `--sysbuild` because sysbuild runs its own CMake process **before** processing the app's CMakeLists.txt.

### 2. MCUboot Flash Size Configuration

MCUboot is built as a separate image in sysbuild and doesn't inherit the app's DTS overlays. It was using the board's default 8MB flash configuration.

### 3. MCUboot Flash Address

MCUboot was being flashed to `0x20000` (the application slot) instead of `0x0` (the boot partition). This happened because the board's DTS sets `zephyr,code-partition = &slot0_partition`, which MCUboot inherited.

## Solution

Create a `sysbuild/` directory in the app with custom configurations for sysbuild and MCUboot.

### Directory Structure

```
app/
├── sysbuild/
│   ├── CMakeLists.txt      # Sysbuild entry point
│   └── mcuboot.overlay     # MCUboot DTS overlay
├── boards/
│   └── esp32c6_devkitc_esp32c6_hpcore.overlay  # App overlay
└── ...
```

### sysbuild/CMakeLists.txt

```cmake
# SPDX-License-Identifier: MIT

set(BOARD esp32c6_devkitc/esp32c6/hpcore)
set(mcuboot_DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/mcuboot.overlay CACHE STRING "" FORCE)

find_package(Sysbuild REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(sysbuild LANGUAGES)
```

Note: Use the fully-qualified board name (`esp32c6_devkitc/esp32c6/hpcore`) to avoid deprecation warnings.

### sysbuild/mcuboot.overlay

```dts
/*
 * Copyright (c) 2025
 * SPDX-License-Identifier: MIT
 *
 * MCUboot overlay for 4MB flash configuration
 */

#include <espressif/esp32c6/esp32c6_wroom_n4.dtsi>
#include <espressif/partitions_0x0_default_4M.dtsi>

/ {
	chosen {
		zephyr,code-partition = &boot_partition;
	};
};
```

Key points:
- `esp32c6_wroom_n4.dtsi` - Sets flash size to 4MB
- `partitions_0x0_default_4M.dtsi` - Uses 4MB partition layout
- `zephyr,code-partition = &boot_partition` - Ensures MCUboot is placed at 0x0

### App Overlay (boards/esp32c6_devkitc_esp32c6_hpcore.overlay)

The app overlay should also include the 4MB configuration:

```dts
#include <espressif/esp32c6/esp32c6_wroom_n4.dtsi>
#include <espressif/partitions_0x0_default_4M.dtsi>

/* ... rest of app-specific overlay ... */
```

## Partition Layout (4MB)

The `partitions_0x0_default_4M.dtsi` defines:

| Partition | Address | Size | Purpose |
|-----------|---------|------|---------|
| mcuboot | 0x0 | 64KB | MCUboot bootloader |
| sys | 0x10000 | 64KB | System data |
| image-0 | 0x20000 | 1792KB | Application slot 0 |
| image-1 | 0x1E0000 | 1792KB | Application slot 1 (OTA) |
| storage | 0x3B0000 | 192KB | LittleFS/NVS storage |
| scratch | 0x3E0000 | 124KB | MCUboot swap scratch |

## Build and Flash

```bash
# Clean build with sysbuild
west build -p always --sysbuild

# Flash (MCUboot to 0x0, app to 0x20000)
west flash
```

## Verification

After flashing, serial output should show:

```
I (40) flash_init: SPI Flash Size : 4MB
```

Instead of 8MB.

## Why This Happens

1. **Sysbuild architecture**: Sysbuild uses `deps/zephyr/share/sysbuild/template/CMakeLists.txt` as the entry point. Creating `app/sysbuild/CMakeLists.txt` overrides this.

2. **MCUboot as separate image**: With sysbuild, MCUboot is configured and built independently. It needs its own overlay to use the correct flash configuration.

3. **Code partition determines flash address**: The `zephyr,code-partition` chosen node controls where the image is linked and flashed. MCUboot needs `&boot_partition` (0x0), while the app uses `&slot0_partition` (0x20000).
