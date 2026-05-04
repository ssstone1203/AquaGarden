# AquaGarden v2（OpenAMP / RPMsg）调试交接说明

**日期**：2026-05-05  
**适用工程路径**：`AquaGarden/hardware/phytiumpi/spi_com/`  
**本文目的**：记录当前问题定位、已验证通断点、错误码与后续建议，供下一轮调试直接使用。

---

## 1. 架构与数据路径（便于对照现象）

| 路径 | Linux 侧 | 到 RA6E2 的 SPI |
|------|-----------|-----------------|
| **v1** | `aqua_spid` → `/dev/spidev0.0` → 内核 SPI 主机驱动 | **主核 Linux 直接** FSPI0 ↔ RA6E2 |
| **v2** | `aqua_rpmsgd` → `/dev/rpmsg_ctrl0` + `ioctl(CREATE_EPT)` + `/dev/rpmsgN` → virtio-rpmsg | **辅助核裸机** `openamp_spi_core0.elf` 内 FSPIM ↔ RA6E2 |

**互斥**：同一时刻只能二选一；`deploy/scripts/switch_to_v1.sh` / `switch_to_v2.sh` 负责停对端、挂/卸 overlay、启相应 daemon 与（v2）remoteproc 加载脚本。

---

## 2. 当前结论概览

- **RA6E2 与飞腾主核之间的 SPI 物理与协议（在 v1 路径下）已验证可用。**
- **v2 路径上：remoteproc 能起来、virtio-rpmsg 能创建 `aqua-spi` 通道，但用户态经 `aqua_rpmsgd` 的 RPMsg 帧收发仍异常**（见 §4）。
- **问题应收敛在「Linux 用户态 ↔ virtio-rpmsg ↔ 辅助核固件」**，而非首先怀疑杜邦线/RA6 固件（v1 已证明通）。

---

## 3. 已验证「是通的」（有运行时或操作证据）

### 3.1 v1（spidev）

- `switch_to_v1.sh` 后 **`aqua_spi_cli sys ping`** 可返回 **`RSP: status=OK ...`**（含合理 `uptime`）。
- 说明：**FSPI0 接线、模式、RA6 从机固件、64B 帧协议、daemon+CLI IPC** 在「主核直驱 SPI」下正常。

### 3.2 v2 基础设施（remoteproc + 通道公告）

- **`/sys/class/remoteproc/remoteproc0/state`** 可为 **`running`**。
- **`dmesg`**（冷启动后）可出现类似链：
  - `Booting fw image openamp_spi_core0.elf`
  - `virtio_rpmsg_bus virtio0: rpmsg host is online`
  - `creating channel aqua-spi addr 0x0`
  - `remote processor homo_rproc is now up`
- **`aqua-openamp-load.service`** 日志可出现：
  - `after: state=running firmware=openamp_spi_core0.elf`
  - `aqua-spi endpoint announced`
- 说明：**辅助核加载、virtio 设备、内核侧 `aqua-spi` 通道创建** 在多数冷启动/正确切换流程下可达。

### 3.3 部署与可执行文件（曾出错点已修）

- **`/opt/aqua/bin/aqua_rpmsgd` 不得为 0 字节**；曾出现 `empty` 导致 **`status=203/EXEC` / Exec format error**，需在板子上 **`make linux-native`** 后 **`cp`** 覆盖。
- **`aqua_daemon_core.c`**：对 **RPMsg + `-ENOMEM`（-12）** 不再累计 `consecutive_err`、**不触发 `reset()`**，避免 **`CREATE_EPT` 风暴**与 `/dev/rpmsgN` 编号暴涨（该逻辑需在板子二进制中实装）。

### 3.4 PSCI / `-4`（部分场景）

- 曾出现 **`can't start rproc homo_rproc: -4`**、`echo start` 阻塞；**冷启动** + **`deploy/scripts/aqua_openamp_load.sh`**（`recovery disabled`、`timeout`、`offline` 分支先 `stop` 再 `start`）后，**可恢复为 `running`**。
- **`deploy/README.md`**、**`openamp_core/README.md`** 已补充相关说明。

---

## 4. 仍有问题 / 未闭合点

### 4.1 用户态 RPMsg 传输（核心悬案）

现场曾出现两类表现（不同阶段/不同负载下）：

| 现象 | 典型含义（用户态返回值约定：失败为 `-errno`） |
|------|-----------------------------------------------|
| **`rpmsg xfer: read … ret=-110`** | **`-ETIMEDOUT`**：写侧可能已发出，**未在超时内读满 64B 应答**（裸机未 `rpmsg_send`、卡在 SPI 双事务、或调度/逻辑未回到读路径）。 |
| **`rpmsg xfer: write … ret=-12`** | **`-ENOMEM`**：内核 **rpmmsg/virtio 发送路径** 分配失败；可能与 **缓冲区耗尽、热切换残留、或驱动限制** 有关；**已与「疯狂 `reset` 新建端点」解耦**后仍可在 **干净 reboot + switch_to_v2** 后复现（见下方 2026-05-05 日志摘要）。 |

**2026-05-05 一组「建议操作」后的摘录（供对照）**：

- `journalctl -u aqua-openamp-load`：`state=running`，`firmware=openamp_spi_core0.elf`，`aqua-spi endpoint announced`。
- `cat /sys/class/remoteproc/remoteproc0/state` → **`running`**。
- **`/opt/aqua/bin/aqua_spi_cli sys ping`** → **`SPI 失败 rc=-12`**（仍为 **ENOMEM 语义**）。
- **同次 `dmesg` 尾部**多为启动期网络/蓝牙等，**未**必包含 ping 瞬时的 rpmsg 打印；下一轮建议在 **执行 ping 同时** 再抓 **`dmesg -Tw`** 或 **`echo 1 > /sys/kernel/debug/dynamic_debug/...`**（若 BSP 开放）以补证据。

### 4.2 辅助核固件行为（与 read 超时强相关）

- 若裸机 **`aqua_spi_slave.c`** 仍在 **Linux `close`/`unbind` 时整机关闭辅助核 / 破坏重连**，会放大 **v2 不稳定**；仓库侧已加入 **「unbind 只 detach、外层循环重建 `rpmsg_create_ept`」** 思路，**必须确认板载 `/lib/firmware/openamp_spi_core0.elf` 与当前源码一致并重编部署**。
- **v1 通、v2 read 超时** 时，优先用 **辅助核 UART 日志**（`AQUA_*`）确认是否进入 **`aqua_rpmsg_cb`**、SPI 双事务是否返回、**`rpmsg_send`** 是否报错。

### 4.3 其它易混点

- **CLI 打印 `rc=-12`**：即 **`-ENOMEM`**，不是正数 `12`；新版 **`aqua_spi_cli`** 可对负 `rc` 附加 **`strerror(-rc)`**（需重新编译安装 CLI）。
- **`stats` 中 `spidev_io_err`**：字段名历史遗留，**v2 下表示后端通用 IO 错误计数**，语义不限于 spidev。

---

## 5. 工程内已做过的相关改动（便于 diff / 回溯）

| 区域 | 内容摘要 |
|------|-----------|
| `linux/aqua_daemon_core.c` | RPMsg 且 **`rc == -ENOMEM`**：不推高 `consecutive_err`、不 **`reset()`**；单次 `LOGW`。 |
| `linux/aqua_backend_rpmsg.c` | `id_before` 在 **`CREATE_EPT`** 之前采样；等新 sysfs id 再 `open`；`write` 路径 `poll`、`discard`；调试 NDJSON（路径含镜像 `/tmp/debug-a3c220.ndjson`）； |
| `openamp_core/src/aqua_spi_slave.c` | **`unbind` 不直接导致整核退场**；**会话结束循环内重建 endpoint**（与 Linux 反复 `ioctl`/连接对齐）。 |
| `deploy/scripts/aqua_openamp_load.sh` | `recovery disabled`；offline/running 换固件分支 **`timeout` + `stop` + sleep + `start`**；失败打印 `dmesg` tail。 |
| `deploy/scripts/switch_to_v2.sh` | 文末 **TIP**：热切换若 **-ENOMEM** 可先 **reboot** 再走 v2。 |
| `deploy/README.md`、`openamp_core/README.md` | `-4`、`read/write`、rpmsg 节点暴增等条目。 |
| `linux/aqua_spi_cli.c` | **`SPI 失败 rc=… (strerror)`** 可读性（需重编 CLI）。 |

（裸机 FSPIM：**`fspim.c`/`aqua_spi_master.c`** 等 stall-bound 等与 SPI 卡点相关改动，仍以仓库为准。）

---

## 6. 建议的后续调试顺序（下一轮）

1. **固定基线**：**冷启动** → **仅** `switch_to_v2.sh` → 确认 **`openamp-load` 成功**、`state=running`。  
2. **确认二进制**：`/opt/aqua/bin/aqua_rpmsgd`、`aqua_spi_cli` 为 **板载 `make linux-native`** 的最新产物，`file` 为 **aarch64 ELF**，**非 empty**。  
3. **分流 write -12 vs read -110**：以 **`journalctl -u aqua-rpmsgd`** 中 **`write`/`read`** 字样为准；**同时进行** **`sudo dmesg -Tw`**（另终端）重现一次 ping，看是否有 **OOM / virtio / rpmsg** 新行。  
4. **辅助核串口**：复现单次 ping，看是否在 **`rpmsg_send` 前卡死或报错**。  
5. **必要时**：对照飞腾 BSP **`homo_rproc`** / virtio-rpmsg **缓冲与端点上限**文档或厂商支持。  
6. **业务兜底**：量产/演示可继续使用 **v1**，直至 v2 用户态收发稳定。

---

## 7. 常用命令速查

```bash
# remoteproc
cat /sys/class/remoteproc/remoteproc0/state
sudo journalctl -u aqua-openamp-load.service -n 30 --no-pager

# 切换路径
sudo /opt/aqua/spi_com/deploy/scripts/switch_to_v1.sh
sudo /opt/aqua/spi_com/deploy/scripts/switch_to_v2.sh

# daemon / CLI
sudo systemctl status aqua-rpmsgd.service --no-pager -l
sudo journalctl -u aqua-rpmsgd.service -n 40 --no-pager
/opt/aqua/bin/aqua_spi_cli sys ping
/opt/aqua/bin/aqua_spi_cli stats

# 板载重编覆盖
cd ~/AquaGarden/hardware/phytiumpi/spi_com && make linux-native
sudo cp -f build/native/aqua_rpmsgd build/native/aqua_spi_cli /opt/aqua/bin/
sudo chmod 755 /opt/aqua/bin/aqua_rpmsgd /opt/aqua/bin/aqua_spi_cli
```

---

## 8. 附：本次附带日志中与「仍失败」一致的要点

- **openamp**：`running` + `openamp_spi_core0.elf` + `aqua-spi announced` ✅  
- **ping**：`rc=-12`（ENOMEM）❌  
- **dmesg 所附片段**：只能说明 **开机阶段 remoteproc/virtio 正常**；**不能**代替 **ping 发生瞬间**的内核日志。

---

*文档结束。下次调试请从此文 §4、§6 接着做。*
