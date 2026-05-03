## AquaGarden SPI 通信工程构建脚本
##
## Linux 端：v1 spidev 直驱版 + v2 OpenAMP/RPMsg 版
##   * 协议层、IPC、CLI、daemon 主循环全部共享，
##     仅"SPI 物理收发"被抽象成 backend 选 spidev / rpmsg
##
## 目标：
##   make test         本机 gcc 跑 PC 单元测试（无需硬件）
##   make linux        交叉编译飞腾派 daemon + CLI（aarch64）
##   make linux-native 直接在飞腾派上编译（cwd 已是 aarch64）
##   make clean
##
## 飞腾派裸机核固件 openamp_spi_core0.elf 不在本 Makefile 范围，
## 它走 phytium-standalone-sdk 的构建系统，详见 openamp_core/README.md。
##
## 工具链：
##   - 本机：gcc
##   - 交叉：aarch64-linux-gnu-gcc（apt install gcc-aarch64-linux-gnu）

CROSS    ?= aarch64-linux-gnu-
CC_HOST  ?= gcc
CC_CROSS ?= $(CROSS)gcc

CFLAGS_COMMON := -O2 -Wall -Wextra -Wno-unused-parameter \
                 -Iinclude -Ilinux -Ilinux/libaqua_spi
LDFLAGS       :=

# ---- 共享 ----
LIB_SRCS    := linux/libaqua_spi/spi_codec.c
CORE_SRCS   := linux/aqua_daemon_core.c \
               linux/aqua_backend_spidev.c \
               linux/aqua_backend_rpmsg.c

# v1 入口
SPID_SRCS   := linux/aqua_spid.c    $(CORE_SRCS) $(LIB_SRCS)
# v2 入口
RPMSGD_SRCS := linux/aqua_rpmsgd.c  $(CORE_SRCS) $(LIB_SRCS)
# CLI（不依赖 backend，只跑 IPC）
CLI_SRCS    := linux/aqua_spi_cli.c $(LIB_SRCS)
# 单元测试
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
linux: $(BUILD_LINUX_DIR)/aqua_spid \
       $(BUILD_LINUX_DIR)/aqua_rpmsgd \
       $(BUILD_LINUX_DIR)/aqua_spi_cli

$(BUILD_LINUX_DIR)/aqua_spid: $(SPID_SRCS) include/spi_protocol.h linux/aqua_backend.h linux/aqua_daemon_core.h linux/libaqua_spi/aqua_ipc.h
	@mkdir -p $(BUILD_LINUX_DIR)
	$(CC_CROSS) $(CFLAGS_COMMON) $(SPID_SRCS) $(LDFLAGS) -o $@

$(BUILD_LINUX_DIR)/aqua_rpmsgd: $(RPMSGD_SRCS) include/spi_protocol.h linux/aqua_backend.h linux/aqua_daemon_core.h linux/libaqua_spi/aqua_ipc.h
	@mkdir -p $(BUILD_LINUX_DIR)
	$(CC_CROSS) $(CFLAGS_COMMON) $(RPMSGD_SRCS) $(LDFLAGS) -o $@

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
	@echo "  make linux         - 交叉编译 v1+v2 daemon + CLI（aarch64）"
	@echo "  make linux-native  - 在飞腾派本机编译"
	@echo "  make clean         - 清理 build/"
	@echo ""
	@echo "可重写变量:"
	@echo "  CROSS=aarch64-linux-gnu-   交叉前缀"
	@echo "  CC_HOST=gcc                本机编译器"
	@echo ""
	@echo "v2 裸机核固件 openamp_spi_core0.elf 走 phytium-standalone-sdk："
	@echo "  详见 openamp_core/README.md"
