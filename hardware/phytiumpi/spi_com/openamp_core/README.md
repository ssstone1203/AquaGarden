# AquaGarden 飞腾派裸机从核固件 (`openamp_spi_core0`)

> **仅当你要走 v2 OpenAMP 路径时才需要本固件**
> v1 spidev 直驱路径（`aqua_spid` daemon）不需要任何裸机核固件。

把飞腾派 4 个 ARMv8 核中**最后一个核**抽出，跑一份纯裸机的 OpenAMP 应用：
- **南向**：直接驱动 FSPI0（`0x2803_A000` / IRQ 191）跟 RA6E2 通信
- **北向**：通过 OpenAMP **RPMsg endpoint `aqua-spi`** 跟主核 Linux 上的 `aqua_rpmsgd` daemon 交换 64 字节帧

> 一句话：本固件就是 [设计_SPI通信工程方案_v2_OpenAMP版.md §0/§5](../设计_SPI通信工程方案_v2_OpenAMP版.md) 中"裸机核"那个方框的所有代码。

---

## 1. 目录结构

```
openamp_core/
├── main.c                  入口：banner + aqua_spi_slave_run()
├── inc/
│   └── aqua_spi_slave.h
├── src/
│   ├── aqua_spi_slave.c    RPMsg endpoint 注册 + 透传分发循环
│   ├── aqua_spi_master.h
│   └── aqua_spi_master.c   FSPIM polling 收发封装
├── common/                 直接拷自 SDK openamp_for_linux 例程
│   ├── memory_layout.h
│   ├── openamp_configs.h
│   └── libmetal_configs.h
├── configs/
│   └── pe2204_aarch64_phytiumpi_aquaspi_core0.config
├── Kconfig
├── ft_openamp.ld           资源表段链接脚本（拷自 SDK，未改）
├── makefile
└── README.md               本文件
```

> 共享代码：`spi_codec.c` / `spi_protocol.h` 不在本目录，
> 而是在 `../linux/libaqua_spi/` 与 `../include/`，由 makefile 通过相对路径直接 include。
> 同一份协议文件被 Linux daemon、PC 单测、本固件三方共用，**永远不会发散**。

---

## 2. 编译前准备

### 2.1 飞腾 SDK
```bash
git clone https://gitee.com/phytium_embedded/phytium-standalone-sdk.git ~/phytium-standalone-sdk
cd ~/phytium-standalone-sdk
. ./set_toolchain.sh        # 装好 phytium 提供的 aarch64 baremetal 工具链
. ./set_kconfig.sh
```

> 工具链找不到时，参考飞腾官方 wiki 装：
> https://gitee.com/phytium_embedded/phytium-standalone-sdk

### 2.2 SDK 路径变量

如果 SDK 不在 `~/phytium-standalone-sdk`，编译时需要传 `SDK_DIR`：
```bash
make SDK_DIR=/绝对路径/phytium-standalone-sdk all -j
```

---

## 3. 编译

```bash
cd openamp_core/

# 第一次：加载默认配置（生成 sdkconfig + sdkconfig.h）
make config_pe2204_phytiumpi_aarch64

# 编译
make all -j

# 输出：openamp_spi_core0.elf  (~ 几百 KB)
ls -lh openamp_spi_core0.elf
```

---

## 4. 烧录到飞腾派

```bash
# 方式 A：scp（需要先在飞腾派开 ssh）
make SCP_TARGET=root@<飞腾派IP> scp

# 方式 B：手动
scp openamp_spi_core0.elf root@<飞腾派IP>:/lib/firmware/
```

烧录后**还不会自动起来**，需要 Linux 主核加载并启动它，详见
[../deploy/README.md](../deploy/README.md) 与
[../设计_SPI通信工程方案_v2_OpenAMP版.md §10 部署步骤](../设计_SPI通信工程方案_v2_OpenAMP版.md)。

简化版：
```bash
echo openamp_spi_core0.elf > /sys/class/remoteproc/remoteproc0/firmware
echo start                  > /sys/class/remoteproc/remoteproc0/state

# Linux 端 daemon
systemctl start aqua-rpmsgd      # autoselect 后会自动选中 rpmsg 后端
journalctl -fu aqua-rpmsgd       # 看到 "已打开 RPMsg endpoint 'aqua-spi'" 即可
```

停止顺序：
```bash
systemctl stop aqua-rpmsgd
echo stop > /sys/class/remoteproc/remoteproc0/state
```

**若 `echo start` 卡住且 `dmesg` 刷屏 `can't start homo_rproc: -4`：**
先关闭内核自动重试并优先走 systemd 封装好的脚本（带 `timeout`、`recovery disabled`、offline 也会先 `stop`）：

```bash
sudo sh -c 'echo disabled > /sys/class/remoteproc/remoteproc0/recovery' 2>/dev/null || true
sudo reboot   # 清理 PSCI 拧巴状态后再试
# 开机后：
sudo systemctl restart aqua-openamp-load.service
```

详见 `../deploy/README.md` §4 故障排查表。

---

## 5. 调试技巧

| 现象 | 排查方向 |
| --- | --- |
| `RPMSG_CREATE_EPT_IOCTL 'aqua-spi' 失败` | 远程核未启动或 endpoint 名不一致；先 `cat /sys/class/remoteproc/remoteproc0/state` 应为 `running`。 |
| `aqua_rpmsgd` 反复 timeout | 远程核启动了但 SPI 不通：先用串口看裸机核日志（飞腾派 UART1 → console 默认 115200 8N1）。 |
| 远程核 banner 没出来 | `firmware` 路径写错（应为 `/lib/firmware/openamp_spi_core0.elf`），或 reserved-memory overlay 没装 → `dmesg \| grep remoteproc`。 |
| 业务字段错乱、CRC OK 但语义全 0 | 协议层有改动，必须**双端同时**重编：v1/v2 daemon + RA6E2 + 本固件。 |

打印走 UART1（飞腾派 4 引脚串口排针），波特率 115200。
所有 `AQUA_I/AQUA_W/AQUA_E` 都可见。

---

## 6. 与设计文档的对照

| 设计文档章节 | 本目录文件 |
| --- | --- |
| §0 系统架构 / §1.4 远程核 FSPIM 初始化 | `src/aqua_spi_master.c` |
| §2.4 双向数据流（CMD + NOP_READ 透传） | `src/aqua_spi_slave.c` 中 `aqua_rpmsg_cb` |
| §5.1 v2 目录结构 | 本目录就是 |
| §5.5 裸机核端关键片段 | `src/aqua_spi_slave.c` 全文 |
| §6 错误处理（SPI 错回 BUSY 伪 RSP） | `aqua_rpmsg_cb` 的 `err_busy` 分支 |
| §9 SDK API 对照表 | 本固件实际用到的 API 调用 |
| §10 部署步骤 | 本 README §4 |
| 附录 A 内存布局 | `common/memory_layout.h` |
