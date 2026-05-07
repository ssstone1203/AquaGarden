#!/usr/bin/env bash
set -euo pipefail

# 一键安装 AquaGarden Python Bridge（飞腾派）
# 约定：
#   - Python 代码：/opt/aqua/bin/aqua_bridge.py
#   - 依赖清单：/opt/aqua/requirements-aqua-bridge.txt
#   - venv：/opt/aqua/venv-bridge

if [[ $EUID -ne 0 ]]; then
  echo "请使用 root 执行：sudo $0"
  exit 1
fi

if [[ ! -f /opt/aqua/bin/aqua_bridge.py ]]; then
  echo "缺少 /opt/aqua/bin/aqua_bridge.py，请先同步该文件到飞腾派。"
  exit 1
fi

if [[ ! -f /opt/aqua/requirements-aqua-bridge.txt ]]; then
  echo "缺少 /opt/aqua/requirements-aqua-bridge.txt，请先同步该文件到飞腾派。"
  exit 1
fi

echo "[1/5] 检查 Python3..."
command -v python3 >/dev/null 2>&1 || { echo "未找到 python3"; exit 1; }

echo "[2/5] 创建虚拟环境..."
python3 -m venv /opt/aqua/venv-bridge

echo "[3/5] 安装依赖..."
/opt/aqua/venv-bridge/bin/pip install --upgrade pip
/opt/aqua/venv-bridge/bin/pip install -r /opt/aqua/requirements-aqua-bridge.txt

echo "[4/5] 安装 systemd service..."
install -m 0644 /opt/aqua/spi_com/deploy/systemd/aqua-bridge.service /etc/systemd/system/aqua-bridge.service
systemctl daemon-reload
systemctl enable aqua-bridge.service

echo "[5/5] 启动服务..."
systemctl restart aqua-bridge.service

echo
echo "安装完成。可用以下命令检查："
echo "  systemctl status aqua-bridge --no-pager"
echo "  journalctl -u aqua-bridge -n 80 --no-pager"
echo "  curl -s http://127.0.0.1:18080/api/status"
