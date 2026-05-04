#!/usr/bin/env bash
# One-shot: install fixed aqua-openamp-load.service (uses aqua_openamp_load.sh)
# and reload systemd. Requires sudo.
# Unit file is loaded from this directory first (aqua-openamp-load.service) so
# /opt rsync of deploy/scripts/ is enough; deploy/systemd/ on device may be stale/root-only.
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
[[ ${EUID:-0} -eq 0 ]] || exec sudo -E bash "$0" "$@"

if [[ -f "$SCRIPT_DIR/aqua-openamp-load.service" ]]; then
	UNIT_SRC="$SCRIPT_DIR/aqua-openamp-load.service"
else
	UNIT_SRC="$SCRIPT_DIR/../systemd/aqua-openamp-load.service"
fi
SH_SRC="$SCRIPT_DIR/aqua_openamp_load.sh"
SH_DST=/opt/aqua/spi_com/deploy/scripts/aqua_openamp_load.sh

install -m644 "$UNIT_SRC" /etc/systemd/system/aqua-openamp-load.service

# 从 /opt 下执行时 SH_SRC 与 SH_DST 常为同一文件，install 会报「为同一文件」
if [[ "$SH_SRC" -ef "$SH_DST" ]]; then
	:
else
	install -m755 "$SH_SRC" "$SH_DST"
fi
systemctl daemon-reload
systemctl reset-failed aqua-openamp-load aqua-rpmsgd 2>/dev/null || true
systemctl restart aqua-openamp-load.service
systemctl restart aqua-rpmsgd.service
echo "[OK] aqua-openamp-load + aqua-rpmsgd restarted"
systemctl is-active aqua-openamp-load aqua-rpmsgd
