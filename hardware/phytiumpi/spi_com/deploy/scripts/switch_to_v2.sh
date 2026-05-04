#!/usr/bin/env bash
# switch_to_v2.sh — 切换到 v2 OpenAMP/RPMsg 路径
#
# 前置条件（脚本里会检查）：
#   - /lib/firmware/openamp_spi_core0.elf 存在
#   - 内核启用了 OPENAMP、RPMSG_CHAR、PHYTIUM_REMOTEPROC（飞腾派 BSP 默认就齐）
#   - /sys/class/remoteproc/remoteproc0 存在
#   - 当前 DTB 是 OpenAMP 版（含 reserved-memory rproc@b0100000 节点）
#     飞腾官方提供 /boot/phytium-pi-board-v3-openamp.dtb，按 ~/open-amp/readme.txt
#     执行 `sudo ln -snf phytium-pi-board-v3-openamp.dtb /boot/phytium-pi-board.dtb && reboot`
#     一次性配置好。
#
# 操作顺序（幂等）：
#   1. 停 v1 daemon + 卸载 spidev overlay（释放 SPI0 IOPad 给裸机核）
#   2. 启动 aqua-openamp-load.service（remoteproc start + insmod rpmsg_char + driver_override）
#   3. 启动 aqua-rpmsgd.service
#
# 用法：sudo ./switch_to_v2.sh
#
# 检查脚本是否成功：
#   journalctl -u aqua-rpmsgd -n 30
#   /opt/aqua/bin/aqua_spi_cli sys ping

set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
    echo "[ERR] 需要 root，请用 sudo 运行" >&2
    exit 1
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SPIDEV_OVERLAY="${SCRIPT_DIR}/../../overlay/install_spidev_overlay.sh"
FW=/lib/firmware/openamp_spi_core0.elf

[[ -f "${FW}" ]] || { echo "[ERR] 缺少裸机核固件 ${FW}（先 scp 过来）" >&2; exit 2; }
[[ -d /sys/class/remoteproc/remoteproc0 ]] || {
    echo "[ERR] /sys/class/remoteproc/remoteproc0 不存在；内核未启用 phytium-remoteproc" >&2; exit 3; }

# 检查 DTB 是否已含 reserved-memory（OpenAMP 版 DTB）
if [[ ! -d /sys/firmware/devicetree/base/reserved-memory/rproc@b0100000 ]]; then
    echo "[ERR] 当前 DTB 没有 reserved-memory/rproc@b0100000 节点。" >&2
    echo "      解决：sudo ln -snf phytium-pi-board-v3-openamp.dtb /boot/phytium-pi-board.dtb && sudo reboot" >&2
    exit 4
fi

echo "[INF] 1/3 停 v1 链路（spid + spidev overlay）..."
systemctl stop aqua-spid.service 2>/dev/null || true
[[ -x "${SPIDEV_OVERLAY}" ]] && "${SPIDEV_OVERLAY}" remove || true

echo "[INF] 2/3 启动远程核（remoteproc start + 加载 rpmsg_char）..."
systemctl restart aqua-openamp-load.service
sleep 0.5
echo -n "      remoteproc state = "; cat /sys/class/remoteproc/remoteproc0/state || true
echo -n "      /dev/rpmsg* :     "; ls /dev/rpmsg* 2>/dev/null || echo "(尚未出现，aqua-rpmsgd 启动时会重试)"

echo "[INF] 3/3 启动 aqua-rpmsgd.service ..."
systemctl restart aqua-rpmsgd.service
sleep 0.5

echo
echo "==== 当前状态 ===="
systemctl --no-pager status aqua-rpmsgd.service | head -12 || true
echo
echo "[OK] 已切换到 v2 (rpmsg)。验证："
echo "     /opt/aqua/bin/aqua_spi_cli sys ping"
echo "     /opt/aqua/bin/aqua_spi_cli stats   # 看 backend 是否走 rpmsg"
echo "[TIP] v1↔v2 热切换后若 journal 出现 write ret=-12（ENOMEM）：先 sudo reboot，"
echo "      再只执行本脚本 + 验证，避免 rpmsg/virtio 残留状态。"
