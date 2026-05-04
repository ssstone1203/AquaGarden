#!/usr/bin/env bash
# 安全覆盖正在运行的 aqua_rpmsgd：必须先让进程退出，否则 Restart=on-failure / 文本文件忙。
#
# 用法：
#   sudo ./safe_install_aqua_rpmsgd.sh [SRC]           # 常规：stop + 临时 drop-in + 拷贝 + start
#   sudo ./safe_install_aqua_rpmsgd.sh --prepare-reboot   # 仅持久 mask + 提示 reboot（不调用 stop，避免超时挂死）
#   sudo ./safe_install_aqua_rpmsgd.sh --after-reboot [SRC] # 开机后：确认无进程 → 拷贝 → unmask → start
#
set -euo pipefail

DEFAULT_SRC=/home/user/AquaGarden/hardware/phytiumpi/spi_com/build/native/aqua_rpmsgd
DST=/opt/aqua/bin/aqua_rpmsgd

this_script() {
	# 供提示语使用
	if [[ "${BASH_SOURCE[0]}" == /* ]]; then
		echo "${BASH_SOURCE[0]}"
	else
		echo "$(pwd)/${BASH_SOURCE[0]}"
	fi
}

if [[ "${1:-}" == "--prepare-reboot" ]]; then
	[[ ${EUID:-0} -eq 0 ]] || exec sudo -E bash "$0" "$@"
	# 勿先 stop：现场若 stop(Result=timeout) 会长时间挂住且仍杀不掉主进程。
	# 持久 mask 后 reboot，内核会清掉所有任务，开机后本 unit 不会自启，便于原地覆盖 DST。
	#
	# 若 /etc/systemd/system/aqua-rpmsgd.service 已是普通文件，systemctl mask 会报
	# "File ... already exists"：先备份再 ln -s /dev/null，效果与 mask 相同（开机不自启）。
	UNIT=/etc/systemd/system/aqua-rpmsgd.service
	UNIT_BAK="${UNIT}.installbak"

	if [[ -L "$UNIT" ]] && [[ "$(readlink -f "$UNIT" 2>/dev/null)" == /dev/null ]]; then
		echo "[OK] 已为 mask 状态（链到 /dev/null），可直接 reboot"
	elif systemctl mask aqua-rpmsgd.service; then
		echo "[info] systemctl mask 成功"
	else
		if [[ -f "$UNIT_BAK" ]]; then
			echo "[err] 已存在 $UNIT_BAK（上次可能中断）。请先执行 --after-reboot 恢复，或确认无用后删除该文件再试。"
			exit 1
		fi
		if [[ -e "$UNIT" && ! -L "$UNIT" ]]; then
			mv "$UNIT" "$UNIT_BAK"
			echo "[info] 单元已备份为 $UNIT_BAK（--after-reboot 时会自动还原）"
		elif [[ -L "$UNIT" ]]; then
			echo "[err] $UNIT 已是符号链接但非 mask：$(readlink "$UNIT")。请手工处理后重试。"
			exit 1
		fi
		ln -sf /dev/null "$UNIT"
		systemctl daemon-reload
		echo "[info] 已手工链到 /dev/null（等价 mask）"
	fi
	systemctl daemon-reload
	systemctl is-enabled aqua-rpmsgd.service 2>/dev/null || true
	echo "[OK] 已持久 mask aqua-rpmsgd.service（/etc → /dev/null）。请执行："
	echo "  sudo reboot"
	echo "开机登录后执行（默认 SRC=$DEFAULT_SRC）："
	echo "  sudo $(this_script) --after-reboot"
	echo "或指定构建产物："
	echo "  sudo $(this_script) --after-reboot /path/to/aqua_rpmsgd"
	exit 0
fi

if [[ "${1:-}" == "--after-reboot" ]]; then
	shift
	[[ ${EUID:-0} -eq 0 ]] || exec sudo -E bash "$0" --after-reboot "$@"
	SRC="${1:-$DEFAULT_SRC}"
	[[ -f "$SRC" ]] || { echo "缺少源文件: $SRC"; exit 1; }
	if pgrep -x aqua_rpmsgd >/dev/null 2>&1; then
		echo "[err] mask+reboot 后不应再有 aqua_rpmsgd。请先取证："
		pgrep -a aqua_rpmsgd || true
		systemctl status aqua-rpmsgd.service --no-pager -l || true
		exit 1
	fi
	cp -f "$SRC" "$DST"
	chmod 755 "$DST"
	rm -f /run/systemd/system/aqua-rpmsgd.service.d/50-install-norestart.conf 2>/dev/null || true

	UNIT=/etc/systemd/system/aqua-rpmsgd.service
	UNIT_BAK="${UNIT}.installbak"
	if [[ -f "$UNIT_BAK" ]]; then
		rm -f "$UNIT"
		mv "$UNIT_BAK" "$UNIT"
		echo "[info] 已从 $UNIT_BAK 恢复单元文件"
	elif [[ -L "$UNIT" ]] && [[ "$(readlink -f "$UNIT" 2>/dev/null)" == /dev/null ]]; then
		systemctl unmask aqua-rpmsgd.service 2>/dev/null || rm -f "$UNIT"
	fi
	systemctl daemon-reload
	systemctl reset-failed aqua-rpmsgd.service 2>/dev/null || true
	systemctl enable aqua-rpmsgd.service 2>/dev/null || true
	systemctl start aqua-rpmsgd.service
	sleep 0.5
	if systemctl is-active --quiet aqua-rpmsgd.service; then
		echo "[OK] aqua-rpmsgd active（已安装 $(readlink -f "$DST") ← $(readlink -f "$SRC")）"
	else
		systemctl status aqua-rpmsgd.service --no-pager -l
		exit 1
	fi
	exit 0
fi

SRC="${1:-$DEFAULT_SRC}"
[[ -f "$SRC" ]] || { echo "缺少源文件: $SRC"; exit 1; }
[[ ${EUID:-0} -eq 0 ]] || exec sudo -E bash "$0" "$SRC"

# 1) Restart=no：避免手工 SIGKILL 后主进程退出后又被 Restart=on-failure 立刻拉起（PID 可能复用）。
# 2) KillSignal=SIGKILL + TimeoutStopSec：stop 阶段直接 SIGKILL，避免卡在 SIGTERM/Timeout。
#    仅写 /run，reboot 即失效；常规成功路径会删该 drop-in 并 daemon-reload。
NORESTART_CONF=/run/systemd/system/aqua-rpmsgd.service.d/50-install-norestart.conf
cleanup_install_state() {
	systemctl unmask aqua-rpmsgd.service 2>/dev/null || true
	rm -f "$NORESTART_CONF" 2>/dev/null || true
	systemctl daemon-reload 2>/dev/null || true
}
trap cleanup_install_state EXIT

mkdir -p "$(dirname "$NORESTART_CONF")"
cat >"$NORESTART_CONF" <<'EOF'
[Service]
Restart=no
KillSignal=SIGKILL
TimeoutStopSec=5
KillMode=control-group
EOF
systemctl daemon-reload
systemctl show -p KillSignal,Restart,TimeoutStopSec aqua-rpmsgd.service --no-pager || true

systemctl stop aqua-rpmsgd.service 2>/dev/null || true
sleep 1
systemctl mask --runtime aqua-rpmsgd.service 2>/dev/null || true
sleep 1

systemctl kill --kill-who=all -s SIGKILL aqua-rpmsgd.service 2>/dev/null || true
sleep 1
systemctl kill --kill-who=all -s SIGKILL aqua-rpmsgd.service 2>/dev/null || true
sleep 1

for _i in 1 2 3; do
	PIDS="$(pgrep -x aqua_rpmsgd 2>/dev/null || true)"
	if [[ -z "${PIDS}" ]]; then break; fi
	echo "[warn] 仍有 aqua_rpmsgd: ${PIDS}，SIGKILL…"
	for p in ${PIDS}; do
		if kill -9 "$p"; then
			echo "[info] 已发 SIGKILL 至 pid=$p"
		else
			echo "[warn] kill -9 $p 失败，退出码 $?"
		fi
	done
	sleep 1
done
if pgrep -x aqua_rpmsgd >/dev/null 2>&1; then
	echo "[err] 仍无法结束 aqua_rpmsgd（与你机上 stop 超时 + SIGKILL 后仍可见进程的现象一致）。"
	echo "请改用无需在运行中杀进程的路径（持久 mask → reboot → 覆盖二进制）："
	echo "  sudo $(this_script) --prepare-reboot"
	echo "  sudo reboot"
	echo "开机后："
	echo "  sudo $(this_script) --after-reboot"
	echo ""
	echo "（排障存档）systemctl status aqua-rpmsgd.service; pgrep -a aqua_rpmsgd"
	ps -o pid,ppid,stat,wchan,cmd -p "$(pgrep -x aqua_rpmsgd | tr '\n' ' ' | sed 's/ $//')" 2>/dev/null || pgrep -a aqua_rpmsgd
	exit 1
fi

systemctl reset-failed aqua-rpmsgd.service 2>/dev/null || true
cp -f "$SRC" "$DST"
chmod 755 "$DST"
rm -f "$NORESTART_CONF"
systemctl daemon-reload
systemctl unmask aqua-rpmsgd.service 2>/dev/null || true
trap - EXIT
systemctl start aqua-rpmsgd.service
sleep 0.5
systemctl is-active --quiet aqua-rpmsgd.service && echo "[OK] aqua-rpmsgd active" || systemctl status aqua-rpmsgd.service --no-pager -l
