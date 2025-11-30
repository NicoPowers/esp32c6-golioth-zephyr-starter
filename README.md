# esp32c6-golioth-zephyr-starter

ESP32-C6 firmware built with Zephyr RTOS and MCUboot.

## Supported Hardware

- ESP32-C6 DevKitC (4MB flash)

> [!NOTE]
> The board overlay (`boards/esp32c6_devkitc_esp32c6_hpcore.overlay`) for LEDs and pressure sensor is just an example, as this was created for a custom PCB based on the ESP32-C6 WROOM-N4.

## Local Setup

> [!IMPORTANT]
> Do not clone this repo directly using `git`. Zephyr's `west` meta tool should be
> used to set up your local workspace.

### Prerequisites

Ensure you have at least these versions of these tools:

- Python 3.10+
- CMake 3.20+

### Initialize workspace

Copy and paste these one at a time:

```shell
# Install and enable `direnv` utility if needed
if ! command -v direnv > /dev/null; then
    sudo apt-get install direnv
fi

if ! grep -q 'direnv hook bash' ~/.bashrc; then
    echo 'eval "$(direnv hook bash)"' >> ~/.bashrc
    . ~/.bashrc
fi
```

```shell
# Create Zephyr project directory and setup Python venv
proj_dir=esp32c6-golioth-zephyr-starter
if [ -e "$proj_dir" ]; then
    echo "$proj_dir already exists, not continuing!"
else
    mkdir esp32c6-golioth-zephyr-starter && \
    cd esp32c6-golioth-zephyr-starter && \
    echo "layout python3" > .envrc && \
    direnv allow
fi
```

```shell
# Download and setup Zephyr project and toolchain
pip install west && \
west init -m git@github.com:NicoPowers/esp32c6-golioth-zephyr-starter.git && \
west update && \
west zephyr-export && \
west packages pip --install && \
west blobs fetch hal_espressif && \
west sdk install --toolchain riscv64-zephyr-elf --version 0.17.4 && \
west config build.board_warn false
```

```shell
# Build the Zephyr app
west build -p always --sysbuild app
```

## Building

Build from the top level of your project. The board target (ESP32-C6 DevKitC)
is configured in `app/sysbuild/CMakeLists.txt`, so no `-b` flag is needed.

```shell
west build -p always --sysbuild app
```

Alternatively, if your current working directory is `app`, do:

```shell
west build -p always --sysbuild
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
