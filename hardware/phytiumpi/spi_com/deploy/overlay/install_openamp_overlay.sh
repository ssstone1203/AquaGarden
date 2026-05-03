#!/usr/bin/env bash
# install_openamp_overlay.sh — 一键编译 + configfs 挂载 reserved-memory overlay
#
# v2 OpenAMP 路径需要这两段预留内存：
#   - 0xb010_0000 ~ 0xb110_0000 (16 MiB)  裸机核固件 ELF
#   - 0xc000_0000 ~ 0xc100_0000 (16 MiB)  RPMsg vring + share buffer
# 必须在 remoteproc start 之前挂上，否则 dmesg 会报
# "remoteproc: failed to load firmware: ENOMEM"
#
# ⚠ 飞腾派官方 BSP 已经在 phytium-pi-board-v3-openamp.dtb 里加了 reserved-memory
#   节点（rproc@b0100000）；如果你的 DTB 已切到 v3-openamp（参考 ~/open-amp/readme.txt），
#   **不需要本 overlay**。本脚本仅在 BSP 没改 DTB 时作 fallback。
#
# 如何判断是否需要：
#   ls /sys/firmware/devicetree/base/reserved-memory/rproc@b0100000  存在=不需要
#
# 用法：
#   sudo ./install_openamp_overlay.sh apply
#   sudo ./install_openamp_overlay.sh remove
#   sudo ./install_openamp_overlay.sh status
#
# 与 ../../overlay/install_spidev_overlay.sh 结构完全一致；这两个 overlay 互斥，
# 不要同时挂（switch_to_v1.sh / switch_to_v2.sh 会自动处理）。

set -euo pipefail

OVERLAY_NAME="aqua_openamp"
OVERLAY_DIR="/sys/kernel/config/device-tree/overlays/${OVERLAY_NAME}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DTS="${SCRIPT_DIR}/phytium_pi_openamp.dts"
DTBO="${SCRIPT_DIR}/phytium_pi_openamp.dtbo"

action="${1:-apply}"

require_root() {
    if [[ ${EUID} -ne 0 ]]; then
        echo "[ERR] 需要 root，请用 sudo 运行" >&2
        exit 1
    fi
}

cmd_status() {
    echo "=== overlay configfs 节点 ==="
    if [[ -d "${OVERLAY_DIR}" ]]; then
        ls -la "${OVERLAY_DIR}/"
        echo
        cat "${OVERLAY_DIR}/status" 2>/dev/null || true
    else
        echo "未挂载（${OVERLAY_DIR} 不存在）"
    fi
    echo
    echo "=== /proc/iomem 中的 reserved 段 ==="
    grep -i 'reserved\|aqua' /proc/iomem || true
    echo
    echo "=== /sys/class/remoteproc/ ==="
    ls -la /sys/class/remoteproc/ 2>/dev/null || echo "（remoteproc 子系统未启用）"
    for d in /sys/class/remoteproc/*/; do
        [[ -d "${d}" ]] || continue
        echo "--- ${d} ---"
        echo -n "  state=    "; cat "${d}state"    2>/dev/null || true
        echo -n "  firmware= "; cat "${d}firmware" 2>/dev/null || true
        echo -n "  name=     "; cat "${d}name"     2>/dev/null || true
    done
}

cmd_remove() {
    require_root
    if [[ -d "${OVERLAY_DIR}" ]]; then
        echo "[INF] 卸载 overlay：${OVERLAY_DIR}"
        rmdir "${OVERLAY_DIR}"
        echo "[OK]  已卸载"
    else
        echo "[INF] overlay 未挂载，跳过"
    fi
}

cmd_apply() {
    require_root

    if [[ ! -f "${DTS}" ]]; then
        echo "[ERR] 找不到 ${DTS}" >&2
        exit 2
    fi

    if [[ ! -d /sys/kernel/config/device-tree/overlays ]]; then
        echo "[ERR] 内核未启用 OF_CONFIGFS 或 configfs 未挂载" >&2
        exit 3
    fi

    if [[ ! -x "$(command -v dtc)" ]]; then
        echo "[ERR] 未找到 dtc，请 'sudo apt install device-tree-compiler'" >&2
        exit 4
    fi

    echo "[INF] 编译 ${DTS} → ${DTBO}"
    dtc -@ -I dts -O dtb -o "${DTBO}" "${DTS}"
    echo "[OK]  编译成功 ($(stat -c %s "${DTBO}") bytes)"

    # 与 v1 spidev overlay 互斥：检测到了就先卸载
    local SPIDEV_OVERLAY="/sys/kernel/config/device-tree/overlays/aqua_spidev0"
    if [[ -d "${SPIDEV_OVERLAY}" ]]; then
        echo "[WARN] 检测到 v1 spidev overlay 仍挂着，先卸载（避免 SPI0 IOPad 冲突）"
        rmdir "${SPIDEV_OVERLAY}"
    fi

    if [[ -d "${OVERLAY_DIR}" ]]; then
        echo "[INF] 旧 overlay 还在，先卸载"
        rmdir "${OVERLAY_DIR}"
    fi

    echo "[INF] 挂载 overlay 到 ${OVERLAY_DIR}"
    mkdir "${OVERLAY_DIR}"
    cat "${DTBO}" > "${OVERLAY_DIR}/dtbo"

    sleep 0.2
    local st
    st="$(cat "${OVERLAY_DIR}/status" 2>/dev/null || echo unknown)"
    echo "[INF] overlay status = ${st}"

    cmd_status
}

case "${action}" in
    apply)  cmd_apply ;;
    remove) cmd_remove ;;
    status) cmd_status ;;
    *) echo "用法：$0 {apply|remove|status}" >&2; exit 1 ;;
esac
