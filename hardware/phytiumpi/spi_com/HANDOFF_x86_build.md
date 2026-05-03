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

## 当前状态 — 主线已完成

> 更新时间：2026-05-04 03:43
>
> v2 OpenAMP/RPMsg 主链路已完成：裸机固件已编译、部署到飞腾派，`aqua-openamp-load` 与
> `aqua-rpmsgd` 已正常运行，CLI `ping` / `stats` / `sensor poll` 已通过。

---

## 本次执行结果

### ✅ 第 1 步：编译裸机固件

x86 主机编译目录：

```bash
cd ~/AquaGarden/hardware/phytiumpi/spi_com/openamp_core
make all -j$(nproc)
```

编译已成功，产物：

```
~/AquaGarden/hardware/phytiumpi/spi_com/openamp_core/pe2204_aarch64_phytiumpi_openamp_spi_core0.elf
```

本地 ELF 校验：

```
33e5970035d76b1f31659f240528d317a41edbadb17b54eb0878f866113f9c0a
```

本次编译过程中已修复：

- `openamp_core/makefile`：`spi_codec.c` 改为相对路径，确保进入 `libuser.a`。
- `phytium-standalone-sdk/soc/common/fmmu_code_table.c`：修复 `num_regions` 静态初始化兼容问题。
- `phytium-standalone-sdk/arch/armv8/aarch64/gcc/fcrt0.S`：将 AArch64 汇编里的 `lr` 改为显式 `x30`。

---

### ✅ 第 2 步：把 `.elf` 传到飞腾派

飞腾派 IP：`192.168.136.165`，用户名：`user`

已传到：

```
/tmp/openamp_spi_core0.elf
```

飞腾派上校验一致：

```
33e5970035d76b1f31659f240528d317a41edbadb17b54eb0878f866113f9c0a  /tmp/openamp_spi_core0.elf
33e5970035d76b1f31659f240528d317a41edbadb17b54eb0878f866113f9c0a  /lib/firmware/openamp_spi_core0.elf
```

如需重新传：

```bash
scp ~/AquaGarden/hardware/phytiumpi/spi_com/openamp_core/pe2204_aarch64_phytiumpi_openamp_spi_core0.elf \
  user@192.168.136.165:/tmp/openamp_spi_core0.elf
```

---

### ✅ 第 3 步：在飞腾派上部署

已完成：

- 固件已部署到 `/lib/firmware/openamp_spi_core0.elf`
- systemd 服务已部署到 `/etc/systemd/system/`
  - `aqua-openamp-load.service`
  - `aqua-rpmsgd.service`
  - `aqua-spid.service`
- Linux 用户态程序已部署到 `/opt/aqua/bin/`
  - `aqua_rpmsgd`
  - `aqua_spid`
  - `aqua_spi_cli`

部署中修复过的关键点：

- `aqua-openamp-load.service` 中 `udevadm` 路径改为 `/usr/bin/udevadm`。
- `aqua-openamp-load.service` 改为幂等启动：目标固件已 running 时不再 stop/start remoteproc。
- `aqua-openamp-load.service` 写入 `driver_override` 后会显式 bind `rpmsg_chrdev`，确保 `/dev/rpmsg_ctrl0` 出现。
- Linux 用户态程序改为静态链接，避免飞腾派 Ubuntu 20.04 上缺少 `GLIBC_2.34`。
- `aqua_backend_rpmsg.c` 不再固定打开 `/dev/rpmsg0`，而是选择 `/sys/class/rpmsg` 中服务名为 `aqua-spi` 的最新 `/dev/rpmsgN`。

---

### ✅ 第 4 步：验证 v2

最终状态：

```bash
systemctl status aqua-openamp-load aqua-rpmsgd --no-pager -l
```

结果：

- `aqua-openamp-load.service`: `active (exited)`
- `aqua-rpmsgd.service`: `active (running)`
- `remoteproc0/state`: `running`
- `remoteproc0/firmware`: `openamp_spi_core0.elf`
- `rpmsg` endpoint：`aqua-spi`

CLI 验证已通过：

```bash
/opt/aqua/bin/aqua_spi_cli ping
/opt/aqua/bin/aqua_spi_cli stats
/opt/aqua/bin/aqua_spi_cli sensor poll
```

已确认结果：

- `ping`: `daemon: alive`
- `sensor poll`: `RSP: status=OK type=SENSOR_DATA`
- `stats`: `rx_rsp_ok` 持续增长，`rx_timeout=0`，`rx_crc_err=0`，`rx_sof_err=0`，`consecutive_err=0`

---

### ✅ 第 5 步：v1 / v2 互斥验证

已完成**安全互斥验证**：

- 尝试启动 `aqua-spid.service` 时，因 `/dev/spidev0.0` 不存在，`ExecStartPre=/usr/bin/test -c /dev/spidev0.0` 失败。
- `Conflicts=aqua-rpmsgd.service` 生效：启动 v1 时 `aqua-rpmsgd` 被停掉。
- 重新启动 `aqua-rpmsgd` 后，v2 立即恢复正常。
- 恢复后 CLI `ping` / `sensor poll` / `stats` 全部通过。

未执行完整 v1 overlay 实机切换。原因：

- 当前 BSP 的 `homo_rproc` 在 `stop` 后可能出现 sysfs 显示 `offline`、但 PSCI 返回 `CPU already on (-4)` 的不一致状态。
- 该状态会导致 `echo start > /sys/class/remoteproc/remoteproc0/state` 卡住或失败，通常需要 reboot 恢复。
- 因此本次没有主动 stop remoteproc 去完整切换 v1 fallback。

如未来必须完整验证 v1 fallback，建议单独安排窗口，并预期可能需要重启飞腾派。

---

## 当前可用命令

### 查看 v2 状态

```bash
systemctl status aqua-openamp-load aqua-rpmsgd --no-pager -l
cat /sys/class/remoteproc/remoteproc0/state
cat /sys/class/remoteproc/remoteproc0/firmware
ls -l /dev/rpmsg_ctrl* /dev/rpmsg* 2>&1
```

### CLI 验证

```bash
/opt/aqua/bin/aqua_spi_cli ping
/opt/aqua/bin/aqua_spi_cli stats
/opt/aqua/bin/aqua_spi_cli sensor poll
```

### 重启 v2 daemon

```bash
sudo systemctl restart aqua-rpmsgd
```

注意：不要随意 `systemctl stop aqua-openamp-load` 后再手动 stop/start remoteproc；当前内核/固件组合下 remoteproc 二次启动有风险。

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
