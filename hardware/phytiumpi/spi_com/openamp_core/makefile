# AquaGarden 飞腾派裸机从核固件 (OpenAMP + FSPIM) 构建脚本
#
# 编译产物：openamp_spi_core0.elf
#   → 烧到 /lib/firmware/ 后由 Linux remoteproc 加载到第 N 个 CPU
#
# 用法：
#   1. 先 source SDK 环境：
#        cd ~/phytium-standalone-sdk && . ./set_toolchain.sh && . ./set_kconfig.sh
#   2. 进本目录加载默认 PHYTIUMPI 配置：
#        make config_pe2204_phytiumpi_aarch64
#   3. 编译：
#        make all -j
#   4. 烧到飞腾派 /lib/firmware/：
#        make scp                        # 默认走 root@192.168.4.51
#      或者：
#        scp openamp_spi_core0.elf root@<飞腾派IP>:/lib/firmware/

PROJECT_DIR = $(CURDIR)
SDK_DIR    ?= $(CURDIR)/../../../../../phytium-standalone-sdk

# 让 SDK 直接用 PATH 里找到的 aarch64-none-elf-gcc，
# 不依赖 AARCH64_CROSS_PATH / install.py / phytium_dev.sh。
# dpkg 安装 gcc-aarch64-none-elf 或 tar 解压工具链后，只要 PATH 里有就行。
export AARCH64_CROSS_PATH ?= $(dir $(shell which aarch64-none-elf-gcc 2>/dev/null))/..
export TOOL_CHAIN_PREFIX  ?= aarch64-none-elf-

# 用户源码：本工程 main + src/ + 共享 spi_codec.c
USER_CSRC  := main.c
USER_CSRC  += $(wildcard src/*.c)
USER_CSRC  += ../linux/libaqua_spi/spi_codec.c

# 头文件搜索路径
USER_INCLUDE += $(PROJECT_DIR) \
                $(PROJECT_DIR)/inc \
                $(PROJECT_DIR)/src \
                $(PROJECT_DIR)/common \
                $(PROJECT_DIR)/../include \
                $(PROJECT_DIR)/../linux/libaqua_spi \
                $(SDK_DIR)/drivers/spi/fspim \
                $(SDK_DIR)/drivers/spi/fspim_v2_0

include $(SDK_DIR)/tools/build/makeall.mk

# 烧录目录（视开发机调整）
USR_BOOT_DIR    ?= /tmp/aqua_firmware

USER_BOOT_IMAGE ?= openamp_spi_core0

# ----- 默认配置加载 -----
#
# 飞腾派 PE2204 64-bit + PHYTIUMPI Firefly 板：
config_pe2204_phytiumpi_aarch64:
	$(MAKE) load_kconfig LOAD_CONFIG_NAME=pe2204_aarch64_phytiumpi_aquaspi_core0

# ----- 一键打包 + 部署 -----
image:
	make clean
	make all -j
	mkdir -p $(USR_BOOT_DIR)
	cp ./$(IMAGE_OUT_NAME).elf $(USR_BOOT_DIR)/$(USER_BOOT_IMAGE).elf

# 远程部署到飞腾派 /lib/firmware/
SCP_TARGET ?= root@192.168.4.51
scp:
	scp $(USR_BOOT_DIR)/$(USER_BOOT_IMAGE).elf $(SCP_TARGET):/lib/firmware/
