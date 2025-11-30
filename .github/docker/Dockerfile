# Minimal Zephyr build container with RISC-V toolchain only
FROM zephyrprojectrtos/ci-base:v0.28.4

ARG ZEPHYR_SDK_VERSION=0.17.4

# Install minimal Zephyr SDK with RISC-V toolchain
RUN wget -q https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v${ZEPHYR_SDK_VERSION}/zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz \
    && tar -xf zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz -C /opt \
    && rm zephyr-sdk-${ZEPHYR_SDK_VERSION}_linux-x86_64_minimal.tar.xz \
    && cd /opt/zephyr-sdk-${ZEPHYR_SDK_VERSION} \
    && ./setup.sh -t riscv64-zephyr-elf -h -c

ENV ZEPHYR_SDK_INSTALL_DIR=/opt/zephyr-sdk-${ZEPHYR_SDK_VERSION}
