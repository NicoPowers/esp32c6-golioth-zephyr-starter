# Radon Fan Alarm Firmware

ESP32-C6 firmware built with Zephyr RTOS and MCUboot.

## Supported Hardware

- ESP32-C6 DevKitC (4MB flash)

## Local Setup

> [!IMPORTANT]
> Do not clone this repo using git. Zephyr's `west` meta tool should be
> used to set up your local workspace.

### Prerequisites

- [Zephyr SDK 0.17.4](https://github.com/zephyrproject-rtos/sdk-ng/releases/tag/v0.17.4) with RISC-V toolchain
- Python 3.10+
- CMake 3.20+
- [direnv](https://direnv.net/)

### Initialize workspace

```shell
mkdir ~/radon-fan-alarm && cd ~/radon-fan-alarm
west init -m git@github.com:NicoPowers/radon-fan-alarm-firmware.git

echo "layout python3" > .envrc
direnv allow

pip install west
west update
west blobs fetch hal_espressif
west zephyr-export
west packages pip --install
```

## Building

Build from the top level of your project. The board target (ESP32-C6 DevKitC)
is configured in `app/sysbuild/CMakeLists.txt`, so no `-b` flag is needed.

```shell
west build -p always --sysbuild app
```

## Flashing

```shell
west flash
```

### Build Outputs

After a successful build:

| File | Description |
|------|-------------|
| `build/mcuboot/zephyr/zephyr.bin` | MCUboot bootloader (flashed to 0x0) |
| `build/app/zephyr/zephyr.signed.bin` | Signed application (for OTA updates) |
| `build/app/zephyr/zephyr.elf` | ELF file (for debugging) |
