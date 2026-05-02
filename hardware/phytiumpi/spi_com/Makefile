## AquaGarden SPI 通信工程构建脚本
##
## 三个目标：
##   1) make test         本机 gcc 跑 PC 单元测试（无需硬件）
##   2) make linux        交叉编译飞腾派 daemon + CLI（aarch64）
##   3) make linux-native 直接在飞腾派上编译（cwd 已是 aarch64）
##
## 默认：make test
##
## 工具链：
##   - 本机：gcc
##   - 交叉：aarch64-linux-gnu-gcc（apt install gcc-aarch64-linux-gnu）

CROSS    ?= aarch64-linux-gnu-
CC_HOST  ?= gcc
CC_CROSS ?= $(CROSS)gcc

CFLAGS_COMMON := -O2 -Wall -Wextra -Wno-unused-parameter \
                 -Iinclude -Ilinux/libaqua_spi
LDFLAGS       :=

LIB_SRCS := linux/libaqua_spi/spi_codec.c

DAEMON_SRCS := linux/aqua_spid.c       $(LIB_SRCS)
CLI_SRCS    := linux/aqua_spi_cli.c    $(LIB_SRCS)
TEST_SRCS   := tests/test_spi_protocol.c $(LIB_SRCS)

BUILD_DIR        := build
BUILD_TEST_DIR   := $(BUILD_DIR)/test
BUILD_LINUX_DIR  := $(BUILD_DIR)/linux
BUILD_NATIVE_DIR := $(BUILD_DIR)/native

# ----- 本机单元测试 -----

.PHONY: test
test: $(BUILD_TEST_DIR)/test_spi_protocol
	@echo "==> 运行单元测试"
	@$(BUILD_TEST_DIR)/test_spi_protocol

$(BUILD_TEST_DIR)/test_spi_protocol: $(TEST_SRCS) include/spi_protocol.h linux/libaqua_spi/spi_codec.h
	@mkdir -p $(BUILD_TEST_DIR)
	$(CC_HOST) $(CFLAGS_COMMON) $(TEST_SRCS) -o $@

# ----- 交叉编译飞腾派可执行 -----

.PHONY: linux
linux: $(BUILD_LINUX_DIR)/aqua_spid $(BUILD_LINUX_DIR)/aqua_spi_cli

$(BUILD_LINUX_DIR)/aqua_spid: $(DAEMON_SRCS) include/spi_protocol.h linux/libaqua_spi/aqua_ipc.h
	@mkdir -p $(BUILD_LINUX_DIR)
	$(CC_CROSS) $(CFLAGS_COMMON) $(DAEMON_SRCS) $(LDFLAGS) -o $@

$(BUILD_LINUX_DIR)/aqua_spi_cli: $(CLI_SRCS) include/spi_protocol.h linux/libaqua_spi/aqua_ipc.h
	@mkdir -p $(BUILD_LINUX_DIR)
	$(CC_CROSS) $(CFLAGS_COMMON) $(CLI_SRCS) $(LDFLAGS) -o $@

# ----- 飞腾派本机编译 -----

.PHONY: linux-native
linux-native:
	@$(MAKE) CC_CROSS=$(CC_HOST) BUILD_LINUX_DIR=$(BUILD_NATIVE_DIR) linux

# ----- 工具 -----

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)

.PHONY: help
help:
	@echo "目标:"
	@echo "  make test          - 本机跑 PC 单元测试"
	@echo "  make linux         - 交叉编译 daemon + CLI（aarch64）"
	@echo "  make linux-native  - 在飞腾派本机编译"
	@echo "  make clean         - 清理 build/"
	@echo ""
	@echo "可重写变量:"
	@echo "  CROSS=aarch64-linux-gnu-   交叉前缀"
	@echo "  CC_HOST=gcc                本机编译器"
