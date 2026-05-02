#!/bin/bash
# 将内核树编好的 phytiumpi_firefly.dtb 安装为 U-Boot base_dtb 所指文件
# （默认 base_dtb=boot/phytium-pi-board.dtb -> phytium-pi-board-v3.dtb）
set -euo pipefail

KERNEL_DIR="${KERNEL_DIR:-$HOME/phytium-linux-kernel}"
SRC_DTB="${SRC_DTB:-$KERNEL_DIR/arch/arm64/boot/dts/phytium/phytiumpi_firefly.dtb}"
DEST_DTB="${DEST_DTB:-/boot/phytium-pi-board-v3.dtb}"
BACKUP_DIR="${BACKUP_DIR:-$HOME/dtb-backups}"

if [[ ! -f "$SRC_DTB" ]]; then
	echo "缺少: $SRC_DTB  （先在内核目录执行: make ARCH=arm64 phytium/phytiumpi_firefly.dtb）" >&2
	exit 1
fi

if [[ ! -w "$(dirname "$DEST_DTB")" ]] && [[ $EUID -ne 0 ]]; then
	exec sudo -E bash "$0" "$@"
fi

mkdir -p "$BACKUP_DIR"
STAMP="$(date +%Y%m%d-%H%M%S)"
if [[ -f "$DEST_DTB" ]]; then
	cp -a "$DEST_DTB" "$BACKUP_DIR/phytium-pi-board-v3.bak-$STAMP"
	echo "已备份: $BACKUP_DIR/phytium-pi-board-v3.bak-$STAMP"
fi

cp -a "$SRC_DTB" "$DEST_DTB"
sync

echo "已写入: $DEST_DTB"
md5sum "$SRC_DTB" "$DEST_DTB"
readlink -f /boot/phytium-pi-board.dtb || true

IMM="${IMMUTABLE:-1}"
if [[ "$IMM" == "1" ]] && command -v chattr >/dev/null; then
	if chattr +i "$DEST_DTB" 2>/dev/null; then
		echo "已设置不可变属性 (chattr +i)。解除: sudo chattr -i $DEST_DTB"
		lsattr "$DEST_DTB"
	else
		echo "未设置 chattr +i（部分文件系统不支持或需手动 sudo chattr +i $DEST_DTB）"
	fi
fi

echo "请确认 U-Boot: printenv base_dtb 仍为 boot/phytium-pi-board.dtb（或指向当前文件）。然后 reboot。"
