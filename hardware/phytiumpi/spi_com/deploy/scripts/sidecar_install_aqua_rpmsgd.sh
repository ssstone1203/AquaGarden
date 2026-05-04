#!/usr/bin/env bash
# 不覆盖「可能仍被占用」的 /opt/aqua/bin/aqua_rpmsgd：新文件新路径 + drop-in 改 ExecStart。
# 解决 ETXTBSY / stop 后 PID 仍存在等异常现场。
set -euo pipefail
SRC="${1:-/home/user/AquaGarden/hardware/phytiumpi/spi_com/build/native/aqua_rpmsgd}"
[[ -f "$SRC" ]] || { echo "缺少源: $SRC"; exit 1; }
[[ ${EUID:-0} -eq 0 ]] || exec sudo -E bash "$0" "$SRC"

TAG="$(date +%Y%m%d%H%M%S)"
NEW="/opt/aqua/bin/aqua_rpmsgd.${TAG}"
DROP=/etc/systemd/system/aqua-rpmsgd.service.d

systemctl stop aqua-rpmsgd.service 2>/dev/null || true
sleep 1

# 主进程已退出但 cgroup 里仍有任务时（PPID=1 的残留），普通 kill 可能杀不干净；
# 让 systemd 对整个 unit cgroup 发 SIGKILL。
systemctl kill --kill-who=all -s SIGKILL aqua-rpmsgd.service 2>/dev/null || true
sleep 1
systemctl kill --kill-who=all -s SIGKILL aqua-rpmsgd.service 2>/dev/null || true
sleep 1

# 仍在跑的实例：再补 SIGKILL
for _i in 1 2 3; do
  PIDS="$(pgrep -x aqua_rpmsgd 2>/dev/null || true)"
  if [[ -z "${PIDS}" ]]; then break; fi
  echo "[warn] 仍有 aqua_rpmsgd: ${PIDS}，SIGKILL…"
  # shellcheck disable=SC2086
  kill -9 ${PIDS} 2>/dev/null || true
  sleep 1
done
if pgrep -x aqua_rpmsgd >/dev/null 2>&1; then
  echo "[err] 仍无法结束 aqua_rpmsgd。请取证后 reboot："
  ps -o pid,ppid,stat,wchan,cmd -p "$(pgrep -x aqua_rpmsgd | tr '\n' ' ')" 2>/dev/null || true
  echo "  cat /proc/$(pgrep -nx aqua_rpmsgd)/cgroup"
  exit 1
fi

install -m755 "$SRC" "$NEW"
mkdir -p "$DROP"
cat >"${DROP}/10-sidecar-exec.conf" <<EOF
# 由 $0 生成：侧车二进制，避免 in-place cp 触发 ETXTBSY
[Service]
ExecStart=
ExecStart=$NEW -f -B auto -p 250
KillMode=control-group
EOF

systemctl daemon-reload
systemctl reset-failed aqua-rpmsgd.service 2>/dev/null || true
systemctl start aqua-rpmsgd.service
sleep 1
systemctl is-active --quiet aqua-rpmsgd.service && echo "[OK] active → $NEW" || { systemctl status aqua-rpmsgd.service --no-pager -l; exit 1; }
