#!/usr/bin/env bash
# switch_to_v1.sh — 切换到 v1 spidev 直驱路径
#
# 操作顺序（幂等）：
#   1. 停 v2 daemon + 远程核 + 卸载 OpenAMP overlay
#   2. 挂载 spidev overlay → /dev/spidev0.0 出现
#   3. 启动 aqua-spid.service
#
# 用法：sudo ./switch_to_v1.sh
#
# 检查脚本是否成功：
#   systemctl status aqua-spid
#   /opt/aqua/bin/aqua_spi_cli sys ping

set -euo pipefail

if [[ ${EUID} -ne 0 ]]; then
    echo "[ERR] 需要 root，请用 sudo 运行" >&2
    exit 1
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SPIDEV_OVERLAY="${SCRIPT_DIR}/../../overlay/install_spidev_overlay.sh"

echo "[INF] 1/3 停 v2 链路（rpmsgd + remoteproc）..."
systemctl stop aqua-rpmsgd.service       2>/dev/null || true
systemctl stop aqua-openamp-load.service 2>/dev/null || true
# DTB 已含 reserved-memory，无需操作 overlay

echo "[INF] 2/3 挂 spidev overlay → /dev/spidev0.0 ..."
"${SPIDEV_OVERLAY}" apply

echo "[INF] 3/3 启动 aqua-spid.service ..."
systemctl restart aqua-spid.service
sleep 0.5

echo
echo "==== 当前状态 ===="
systemctl --no-pager status aqua-spid.service | head -12 || true
echo
echo "[OK] 已切换到 v1 (spidev)。验证："
echo "     /opt/aqua/bin/aqua_spi_cli sys ping"
