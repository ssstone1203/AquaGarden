# AquaGarden SPI 通信 — 部署辅助文件

本目录给"飞腾派端"打包了 v1 与 v2 路径所需的全部 systemd / overlay / 脚本，
**只在生产部署或集成测试时使用**，开发态 `make linux-native` 直接跑前台 daemon 不需要这些。

```
deploy/
├── overlay/
│   ├── phytium_pi_openamp.dts          v2 reserved-memory overlay 源文件
│   └── install_openamp_overlay.sh      编译并 configfs 挂载该 overlay
├── systemd/
│   ├── aqua-spid.service               v1 daemon (Linux spidev)
│   ├── aqua-rpmsgd.service             v2 daemon (OpenAMP/RPMsg, autoselect)
│   └── aqua-openamp-load.service       挂 overlay + remoteproc start，v2 必需
└── scripts/
    ├── switch_to_v1.sh                 一键切到 v1
    └── switch_to_v2.sh                 一键切到 v2
```

> v1 的 spidev overlay (`phytium_pi_spidev0.dts`) 仍在原 `../overlay/`，
> 与本目录脚本互相引用；不要重复拷贝。

---

## 1. 一次性安装到飞腾派

下面假设把整个 `spi_com/` 子树同步到飞腾派 `/opt/aqua/spi_com/`，
二进制装到 `/opt/aqua/bin/`。

```bash
# 在开发机上：
cd AquaGarden/hardware/phytiumpi
make -C spi_com linux                       # 交叉编译 v1+v2 daemon + CLI
rsync -av spi_com/ root@<飞腾派IP>:/opt/aqua/spi_com/
rsync -av spi_com/build/linux/{aqua_spid,aqua_rpmsgd,aqua_spi_cli} root@<飞腾派IP>:/opt/aqua/bin/

# 在飞腾派上：
cd /opt/aqua/spi_com/openamp_core         # 仅 v2 路径需要：上传裸机核固件
# （固件需在带 phytium-standalone-sdk 的开发机上编出来，scp 到 /lib/firmware/）

cp deploy/systemd/*.service /etc/systemd/system/
systemctl daemon-reload
```

---

## 2. v1 / v2 切换

互斥的两条 service：
| 路径 | 拉起的单元 | 物理通道 |
| --- | --- | --- |
| **v1** | `aqua-spid.service` | `/dev/spidev0.0` → FSPI0 → RA6E2 |
| **v2** | `aqua-openamp-load.service` + `aqua-rpmsgd.service` | RPMsg → 裸机核 → FSPI0 → RA6E2 |

`Conflicts=` 已经写在 service 文件里，systemd 会自动停对面，但两个 overlay
（spidev / openamp）也要切换，所以推荐用脚本：

```bash
sudo /opt/aqua/spi_com/deploy/scripts/switch_to_v1.sh
# 或
sudo /opt/aqua/spi_com/deploy/scripts/switch_to_v2.sh
```

切换完后任意路径都可以用同一个 CLI 验证：
```bash
/opt/aqua/bin/aqua_spi_cli sys ping
/opt/aqua/bin/aqua_spi_cli stats
/opt/aqua/bin/aqua_spi_cli snapshot
```

---

## 3. v2 路径前置条件清单

`switch_to_v2.sh` 启动前会检查；若失败按顺序补齐：

1. **裸机核固件**
   `/lib/firmware/openamp_spi_core0.elf` 必须存在。
   构建：`cd openamp_core && make config_pe2204_phytiumpi_aarch64 && make -j && make scp`
   ⚠ 飞腾派**默认装的是 SDK 自带 echo 例程的固件** `openamp_core0.elf`，与本工程无关；
   `aqua-openamp-load.service` 在启动时把 firmware 名指到我们的 `openamp_spi_core0.elf`，
   stop 时自动恢复回原值，所以不破坏官方 echo demo（`~/open-amp/rpmsg-demo`）。
2. **内核 remoteproc / rpmsg / OF_OVERLAY 框架**
   `/sys/class/remoteproc/remoteproc0` 必须存在 + `rpmsg_char.ko` 可加载
   飞腾派 BSP `5.10.209-phytium-embedded` 默认就齐，无需重编。
3. **OpenAMP 版 DTB**
   当前活跃 DTB 必须包含 `reserved-memory/rproc@b0100000` 节点。
   验证：`ls /sys/firmware/devicetree/base/reserved-memory/rproc@b0100000`
   若不存在，按飞腾官方步骤一次性切换：
   ```bash
   sudo ln -snf phytium-pi-board-v3-openamp.dtb /boot/phytium-pi-board.dtb
   sudo reboot
   ```
   （详见 `~/open-amp/readme.txt`）
4. **OF_CONFIGFS（可选，仅 fallback overlay 用）**
   `/sys/kernel/config/device-tree/overlays/` 可写。本工程默认走"DTB 已含 reserved-memory"
   路径，不挂 overlay；只有在 BSP 没改 DTB 时才 `install_openamp_overlay.sh apply`。

---

## 4. 故障排查

| 现象 | 排查 |
| --- | --- |
| `systemctl status aqua-rpmsgd` 报 `cannot open /dev/rpmsg_ctrl0` | 远程核没起来 → `cat /sys/class/remoteproc/remoteproc0/state`，应为 `running`；若是 `offline` 则看 `dmesg \| tail -50`。 |
| **`dmesg`：`can't start rproc homo_rproc: -4`、`Boot failed: -4`；或对 `state` 写 `start` 时 shell 卡住** | **勿手搓连写 start**（易触发 BSP 自动重试，阻塞 sysfs）。先：`sudo sh -c 'echo disabled > /sys/class/remoteproc/remoteproc0/recovery'`，再 `sudo reboot`。开机后用 **`sudo systemctl restart aqua-openamp-load.service`**（或 **`sudo ./deploy/scripts/switch_to_v2.sh`**），不要用无 `timeout` 的裸 `echo start`。`-4` 多与 **PSCI / 核电源状态与 sysfs `offline` 不一致**有关，整机冷启动最常恢复。 |
| 远程核 `failed to load firmware: ENOMEM` | DTB 没含 reserved-memory → 按 §3.3 切到 v3-openamp DTB；fallback：`install_openamp_overlay.sh apply`（极少需要）。 |
| `/dev/rpmsg0` 没出现但 remoteproc=running | rpmsg_char.ko 没加载或 driver_override 没设：`bash ~/open-amp/set_env.sh` 走一遍模拟，看哪步失败；或重启 `aqua-openamp-load.service`。 |
| daemon 启动后 `连续 5 次错，重置 backend` | SPI 物理层接错 / 速率太高 / RA6E2 没烧固件。先切到 v1 用示波器验证。 |
| journal：`rpmsg xfer … ret=-12`（即 **`-ENOMEM`**）且 `/dev/rpmsg` 编号暴涨 | **旧逻辑在 ENOMEM 上仍 `reset()` → 反复 `CREATE_EPT`**，雪上加霜。换 **新版本 `aqua_rpmsgd`（RPMsg ENOMEM 不触发 auto-reset）** + **冷启动** 清理节点；裸机 **`openamp_spi_core0.elf`** 须为 **Linux 断连后不整机下线的重连版本**。 |
| 切回 v1 后 `/dev/spidev0.0` 没出现 | spidev overlay 没挂回去 → `install_spidev_overlay.sh apply`。 |

更详细的内核侧步骤：
[../设计_SPI通信工程方案_v2_OpenAMP版.md §10 部署步骤](../设计_SPI通信工程方案_v2_OpenAMP版.md)
