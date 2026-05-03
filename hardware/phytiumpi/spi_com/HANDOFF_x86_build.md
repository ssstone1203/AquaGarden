# AquaGarden SPI v2 (OpenAMP) — x86 编译交接文档

## 背景

本项目在飞腾派（PhytiumPi PE2204）上实现 OpenAMP + RPMsg + FSPIM 的 SPI 通信方案（v2），
替代原有 Linux spidev 直驱方案（v1，仍保留为 fallback）。

x86 主机（`davidwang-virtual-machine`）负责**交叉编译裸机固件**，然后 scp 到飞腾派部署。

---

## 已完成的步骤

### ✅ 代码已全部写完（在 x86 主机 `~/AquaGarden/`）

| 目录/文件 | 说明 |
|---|---|
| `linux/aqua_backend.h` | 后端抽象接口（spidev / rpmsg 通用） |
| `linux/aqua_backend_spidev.c` | v1 spidev 后端实现 |
| `linux/aqua_backend_rpmsg.c` | v2 RPMsg 后端实现 |
| `linux/aqua_daemon_core.{h,c}` | 两个 daemon 共用核心逻辑 |
| `linux/aqua_spid.c` | v1 daemon 入口（refactored） |
| `linux/aqua_rpmsgd.c` | v2 daemon 入口（新增） |
| `openamp_core/` | 裸机从核固件（OpenAMP + FSPIM SPI Master） |
| `deploy/systemd/` | systemd 服务文件（v1/v2 互斥） |
| `deploy/scripts/` | `switch_to_v1.sh` / `switch_to_v2.sh` |

### ✅ 飞腾派环境已确认

- `remoteproc` / `rpmsg` 内核模块已有
- DTB 中已预置 `rproc@b0100000` reserved-memory，无需额外 overlay
- `phytium-standalone-sdk` 在飞腾派 `~/phytium-standalone-sdk/`

### ✅ x86 主机编译器已就绪

```
aarch64-none-elf-gcc (Linaro GCC 7.3-2018.05) 7.3.1
```

---

## 当前卡点 — 需要在 x86 上完成

### 第 1 步：编译裸机固件（**当前任务**）

```bash
cd ~/AquaGarden/hardware/phytiumpi/spi_com/openamp_core
make all -j$(nproc) 2>&1 | tail -40
```

**注意事项：**
- `sdkconfig` 已存在，不需要跑 `menuconfig`
- `phytium-standalone-sdk` 路径：`~/phytium-standalone-sdk`（makefile 里 `SDK_DIR` 会自动找到）
- 不需要执行 `install.py`，不需要 `source` 任何环境变量
- `TOOL_CHAIN_PREFIX` 已在 makefile 里覆盖为 `aarch64-none-elf-`，直接用 PATH 里的编译器

**编译成功后产物：**
```
openamp_core/image/openamp_spi_core0.elf   （或类似路径，看 build 输出）
```

**如果报错 `fspim.h: No such file or directory`：**
已修复（在 `makefile` 的 `USER_INCLUDE` 里加了 `$(SDK_DIR)/drivers/spi/fspim`），重试即可。

**如果 `SDK_DIR` 找不到：**
```bash
make all -j$(nproc) SDK_DIR=$HOME/phytium-standalone-sdk
```

---

### 第 2 步：把 .elf 传到飞腾派

飞腾派 IP：`192.168.136.165`，用户名：`user`

```bash
# 先确认 .elf 在哪
find ~/AquaGarden/hardware/phytiumpi/spi_com/openamp_core -name "*.elf"

# scp 传过去
scp <elf路径> user@192.168.136.165:/tmp/openamp_spi_core0.elf
```

---

### 第 3 步：在飞腾派上部署（SSH 进去操作）

```bash
ssh user@192.168.136.165

# 放到 remoteproc firmware 目录
sudo cp /tmp/openamp_spi_core0.elf /lib/firmware/openamp_spi_core0.elf

# 安装 systemd 服务（从 x86 scp 过来）
# 服务文件在 ~/AquaGarden/hardware/phytiumpi/spi_com/deploy/systemd/
sudo cp aqua-openamp-load.service aqua-rpmsgd.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable aqua-openamp-load
sudo systemctl start aqua-openamp-load
```

---

### 第 4 步：验证 v2（飞腾派上）

```bash
# 查看 remoteproc 是否加载成功
cat /sys/class/remoteproc/remoteproc0/state

# 查看 rpmsg 设备是否出现
ls /dev/rpmsg*

# 启动 v2 daemon
sudo systemctl start aqua-rpmsgd

# 用 CLI 测试
aqua_spi_cli ping
aqua_spi_cli poll
```

---

### 第 5 步：v1 / v2 互切验证互斥

```bash
# 切到 v1
sudo bash ~/AquaGarden/.../deploy/scripts/switch_to_v1.sh

# 切回 v2
sudo bash ~/AquaGarden/.../deploy/scripts/switch_to_v2.sh
```

---

## 关键路径

```
~/AquaGarden/hardware/phytiumpi/spi_com/
├── openamp_core/          # 裸机固件源码 + makefile
│   ├── makefile           # 已修改：不依赖 install.py，TOOL_CHAIN_PREFIX 用 PATH
│   ├── sdkconfig          # 已生成，CONFIG_USE_SPI=y CONFIG_USE_FSPIM=y
│   └── configs/pe2204_aarch64_phytiumpi_aquaspi_core0.config
├── linux/                 # Linux 用户态 daemon 源码
├── deploy/
│   ├── systemd/           # aqua-spid.service aqua-rpmsgd.service aqua-openamp-load.service
│   └── scripts/           # switch_to_v1.sh switch_to_v2.sh
~/phytium-standalone-sdk/  # SDK（x86 主机本地，makefile 用它编译）
```
