#!/usr/bin/env bash
# sync_from_canonical.sh — 把规范源 (../include / ../linux/libaqua_spi)
# 同步到 RA6E2 工程的 patch 目录
#
# 用法：bash sync_from_canonical.sh
#
# 这是因为 e2studio 工程通常按"复制源文件进项目"管理，无法用符号链接。
# 任何对 spi_protocol.h / spi_codec.{h,c} 的修改都必须先改 ../include 或
# ../linux/libaqua_spi 的"规范源"，再运行本脚本同步过来。

set -euo pipefail
cd "$(dirname "$0")"

CANONICAL_INC=../include
CANONICAL_LIB=../linux/libaqua_spi

cp -v "$CANONICAL_INC/spi_protocol.h"       ./spi_protocol.h
cp -v "$CANONICAL_LIB/spi_codec.h"          ./spi_codec.h
cp -v "$CANONICAL_LIB/spi_codec.c"          ./spi_codec.c

cat > .last_sync.txt <<EOF
last sync time: $(date -u '+%Y-%m-%d %H:%M:%S UTC')
canonical_inc:  $(realpath "$CANONICAL_INC")
canonical_lib:  $(realpath "$CANONICAL_LIB")
EOF

echo "完成。请在 e2studio 中刷新工程目录，使新文件被识别。"
