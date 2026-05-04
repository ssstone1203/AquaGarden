#!/usr/bin/env bash
# Run by systemd (root). Loads openamp_spi_core0.elf, waits for aqua-spi rpmsg,
# binds rpmsg_char. Single script avoids systemd multiline ExecStart parsing bugs.
set -euo pipefail

RP=/sys/class/remoteproc/remoteproc0
FW_AQUA=openamp_spi_core0.elf

log() { echo "[aqua-openamp-load] $*"; }

# Disable BSP auto-recovery (can wedge on boot failure)
echo disabled >"$RP/recovery" 2>/dev/null || true

cur=$(cat "$RP/state" 2>/dev/null || echo unknown)
cur_fw=$(cat "$RP/firmware" 2>/dev/null || echo unknown)
log "before: state=$cur firmware=$cur_fw"

shopt -s nullglob

case "$cur:$cur_fw" in
running:${FW_AQUA})
    log "already running target firmware, skip stop/start"
    ;;
running:*)
    log "running but fw=$cur_fw -> stop + swap + start"
    timeout 3 sh -c "echo stop >$RP/state" 2>/dev/null \
        || log "stop timed out or failed (BSP quirk)"
    sleep 0.2
    echo "$FW_AQUA" >"$RP/firmware"
    timeout 5 sh -c "echo start >$RP/state" 2>/dev/null \
        || { log "start failed; try reboot"; exit 1; }
    ;;
offline:*|crashed:*|suspended:*)
    log "state=$cur -> write firmware + start"
    echo "$FW_AQUA" >"$RP/firmware"
    timeout 5 sh -c "echo start >$RP/state" 2>/dev/null \
        || { log "start failed; try reboot"; exit 1; }
    ;;
*)
    log "unknown state=$cur; not touching remoteproc" >&2
    exit 2
    ;;
esac

cur=$(cat "$RP/state" 2>/dev/null || echo unknown)
cur_fw=$(cat "$RP/firmware" 2>/dev/null || echo unknown)
log "after: state=$cur firmware=$cur_fw"

for i in $(seq 1 10); do
    sleep 0.5
    if ls /sys/bus/rpmsg/devices/virtio0.aqua-spi.* >/dev/null 2>&1; then
        log "aqua-spi endpoint announced"
        break
    fi
    if [ "$i" -eq 10 ]; then
        log "[WARN] aqua-spi endpoint not seen in 5s"
    fi
done

modprobe rpmsg_char

for d in /sys/bus/rpmsg/devices/virtio0.aqua-spi.*/driver_override; do
    [ -e "$d" ] && echo rpmsg_chrdev >"$d"
done

for dev in /sys/bus/rpmsg/devices/virtio0.aqua-spi.*; do
    [ -e "$dev" ] || continue
    if [ -e "$dev/driver" ]; then
        drv=$(readlink -f "$dev/driver" 2>/dev/null || true)
        case "$drv" in *rpmsg_chrdev*) continue ;; esac
    fi
    echo "$(basename "$dev")" >/sys/bus/rpmsg/drivers/rpmsg_chrdev/bind || true
done

udevadm settle

log "final: state=$(cat "$RP/state") rpmsg_devs=$(ls /dev/rpmsg* 2>/dev/null | tr '\n' ' ')"
