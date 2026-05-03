#!/usr/bin/env bash
# install_spidev_overlay.sh — 一键编译 + configfs 在线挂载 spidev overlay
#
# 用法：
#   sudo ./install_spidev_overlay.sh apply     # 编译并挂载（默认）
#   sudo ./install_spidev_overlay.sh remove    # 卸载
#   sudo ./install_spidev_overlay.sh status    # 看当前状态
#
# 本脚本是幂等的：重复 apply 会先 remove 再 apply。

set -euo pipefail

OVERLAY_NAME="aqua_spidev0"
OVERLAY_DIR="/sys/kernel/config/device-tree/overlays/${OVERLAY_NAME}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DTS="${SCRIPT_DIR}/phytium_pi_spidev0.dts"
DTBO="${SCRIPT_DIR}/phytium_pi_spidev0.dtbo"

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
        echo "--- status ---"
        cat "${OVERLAY_DIR}/status" 2>/dev/null || true
    else
        echo "未挂载（${OVERLAY_DIR} 不存在）"
    fi
    echo
    echo "=== /dev/spidev* ==="
    ls -la /dev/spidev* 2>/dev/null || echo "（不存在）"
    echo
    echo "=== /sys/bus/spi/devices/ ==="
    ls -la /sys/bus/spi/devices/
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

    if [[ -d "${OVERLAY_DIR}" ]]; then
        echo "[INF] 旧 overlay 还在，先卸载"
        rmdir "${OVERLAY_DIR}"
    fi

    echo "[INF] 挂载 overlay 到 ${OVERLAY_DIR}"
    mkdir "${OVERLAY_DIR}"

    # 写入 dtbo 属性会立即触发 of_overlay_apply()。失败时 status 会留下错误，
    # 我们写入后立刻读 status 做一次校验。
    cat "${DTBO}" > "${OVERLAY_DIR}/dtbo"

    sleep 0.2  # 给内核 udev 一点时间创建 /dev/spidev*

    local st
    st="$(cat "${OVERLAY_DIR}/status" 2>/dev/null || echo unknown)"
    echo "[INF] overlay status = ${st}"

    if ls /dev/spidev0.* >/dev/null 2>&1; then
        echo "[OK]  /dev/spidev* 已就绪："
        ls -la /dev/spidev0.*
    else
        echo "[WARN] overlay 已挂载但 /dev/spidev0.* 仍不存在；运行 'status' 子命令查看详情"
        cmd_status
        exit 5
    fi
}

case "${action}" in
    apply)  cmd_apply ;;
    remove) cmd_remove ;;
    status) cmd_status ;;
    *) echo "用法：$0 {apply|remove|status}" >&2; exit 1 ;;
esac
