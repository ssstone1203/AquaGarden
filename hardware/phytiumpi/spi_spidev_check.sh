#!/bin/bash
# 飞腾派 SPI0 / spidev 自检（无设备节点时运行）
set +e
echo "=== /dev/spidev* ==="
ls -la /dev/spidev* 2>&1
echo
echo "=== SPI 设备 (sysfs) ==="
ls -la /sys/bus/spi/devices/ 2>&1
echo
echo "=== 已加载模块 ==="
lsmod | grep -E 'spidev|spi_phytium|phytium_spi' || true
echo
echo "=== 设备树中 spidev 节点 ==="
find /proc/device-tree -name 'spidev*' 2>/dev/null | head -20
echo
echo "=== spi@2803a000 状态 (十六进制) ==="
for f in /proc/device-tree/soc/spi@2803a000/status \
         /proc/device-tree/spi@2803a000/status; do
	if [[ -f "$f" ]]; then
		echo -n "$f: "
		cat "$f" | tr -d '\0'; echo
	fi
done
echo
echo "=== dmesg (spi / spidev / fdt / overlay) ==="
dmesg | grep -iE 'spi@2803a|phytium_spi|spidev|fdt apply|overlay|Error.*fdt' | tail -35
echo
echo "=== 尝试加载 spidev 模块后再列节点 ==="
if [[ -e /proc/modules ]] && ! grep -q '^spidev ' /proc/modules; then
	modprobe spidev 2>&1 || sudo modprobe spidev 2>&1
fi
ls -la /dev/spidev* 2>&1
echo "完成。"
