#!/usr/bin/env bash
#
# rx888_preflight.sh — make sure the host is ready to stream from an RX888.
#
# Checks (in order):
#   1. RX888 device is present (lsusb)
#   2. Connected at SuperSpeed (USB3, blue port)
#   3. FX3 firmware is loaded (PID 0x00f1, not bootloader 0x00f3)
#   4. usbfs_memory_mb is large enough for our queue depth
#   5. Current user can access the device (udev rules or root)
#
# Anything that can be fixed automatically (with sudo) is fixed.
# Anything that can't is reported with a one-line remediation hint.
#
# Exit code: 0 if everything is green, non-zero otherwise.
# Pass --quiet to suppress green ticks; failures always print.
#
# SPDX-License-Identifier: GPL-3.0-or-later

set -uo pipefail

VID=04b4
PID_BOOT=00f3
PID_APP=00f1

# Default firmware locations; first one that exists wins.
FW_CANDIDATES=(
    "/usr/local/share/rx888_tools/firmware/SDDC_FX3.img"
    "/usr/share/rx888_tools/firmware/SDDC_FX3.img"
    "${RX888_FIRMWARE:-}"
)

USBFS_MIN_MB=128

QUIET=0
[[ "${1:-}" == "--quiet" ]] && QUIET=1

ok()   { [[ $QUIET -eq 1 ]] || echo "  ✓ $*"; }
info() { [[ $QUIET -eq 1 ]] || echo "  · $*"; }
warn() { echo "  ! $*" >&2; }
fail() { echo "  ✗ $*" >&2; }

step() { [[ $QUIET -eq 1 ]] || echo; echo "[$1] $2"; }

errors=0

# --- 1. Device present ---------------------------------------------------
step 1 "RX888 USB device"
if ! command -v lsusb >/dev/null; then
    fail "lsusb not found (apt install usbutils)"
    exit 2
fi

dev_line="$(lsusb -d "${VID}:" 2>/dev/null | head -1)"
if [[ -z "$dev_line" ]]; then
    fail "no Cypress FX3 device (${VID}:*) found on USB. Is the RX888 plugged in?"
    exit 2
fi
ok "found: $dev_line"

bus=$(echo "$dev_line"  | awk '{print $2}')
devn=$(echo "$dev_line" | awk '{print $4}' | tr -d ':')
pid=$(echo  "$dev_line" | awk '{print $6}' | cut -d: -f2)

# --- 2. SuperSpeed link --------------------------------------------------
step 2 "USB SuperSpeed (USB3) link"
speed_file="/sys/bus/usb/devices/${bus#0}-*/speed"
# match any device on this bus by walking children; simpler: shell-glob
speed=""
for f in /sys/bus/usb/devices/*/idVendor; do
    if [[ -r "$f" ]] && [[ "$(cat "$f")" == "$VID" ]]; then
        d="$(dirname "$f")"
        if [[ -r "$d/idProduct" ]] && [[ "$(cat "$d/idProduct")" == "$pid" ]]; then
            speed="$(cat "$d/speed" 2>/dev/null || true)"
            break
        fi
    fi
done

if [[ -z "$speed" ]]; then
    warn "could not read link speed from sysfs; assuming OK"
elif [[ "$speed" == "5000" ]] || [[ "$speed" == "10000" ]]; then
    ok "link speed ${speed} Mbps (SuperSpeed)"
else
    fail "link speed ${speed} Mbps — RX888 needs USB3 (5000+ Mbps). Move to a blue port."
    errors=$((errors+1))
fi

# --- 3. Firmware loaded --------------------------------------------------
step 3 "FX3 firmware loaded"
if [[ "$pid" == "$PID_APP" ]]; then
    ok "device is in app mode (${VID}:${PID_APP})"
elif [[ "$pid" == "$PID_BOOT" ]]; then
    info "device is in bootloader mode (${VID}:${PID_BOOT}); attempting firmware upload..."
    fw=""
    for c in "${FW_CANDIDATES[@]}"; do
        [[ -n "$c" && -f "$c" ]] && { fw="$c"; break; }
    done
    if [[ -z "$fw" ]]; then
        fail "no firmware image found. Set RX888_FIRMWARE=/path/to/SDDC_FX3.img"
        errors=$((errors+1))
    elif ! command -v rx888_stream >/dev/null; then
        fail "rx888_stream not in PATH; cannot upload firmware. Install rx888-tools."
        errors=$((errors+1))
    else
        info "uploading $fw"
        # rx888_stream loads firmware then exits if no streaming requested;
        # our trick: pipe stdout to dd of=/dev/null, kill after 2s.
        timeout 3 rx888_stream -f "$fw" >/dev/null 2>&1 || true
        sleep 1
        new_pid="$(lsusb -d "${VID}:" 2>/dev/null | awk '{print $6}' | cut -d: -f2)"
        if [[ "$new_pid" == "$PID_APP" ]]; then
            ok "firmware loaded; device now ${VID}:${PID_APP}"
            pid="$PID_APP"
        else
            fail "firmware upload did not produce app-mode device (now ${VID}:${new_pid:-?})"
            errors=$((errors+1))
        fi
    fi
else
    fail "unexpected PID ${VID}:${pid} (expected ${PID_APP} or ${PID_BOOT})"
    errors=$((errors+1))
fi

# --- 4. usbfs_memory_mb --------------------------------------------------
step 4 "kernel usbfs_memory_mb"
USBFS_FILE=/sys/module/usbcore/parameters/usbfs_memory_mb
if [[ ! -r "$USBFS_FILE" ]]; then
    warn "$USBFS_FILE not readable; cannot verify"
else
    cur=$(cat "$USBFS_FILE")
    if [[ "$cur" -ge "$USBFS_MIN_MB" ]]; then
        ok "usbfs_memory_mb = $cur (>= ${USBFS_MIN_MB})"
    else
        info "usbfs_memory_mb = $cur, raising to ${USBFS_MIN_MB}..."
        if echo "$USBFS_MIN_MB" | sudo tee "$USBFS_FILE" >/dev/null 2>&1; then
            ok "raised to $(cat "$USBFS_FILE")"
        else
            fail "failed to raise usbfs_memory_mb. Run as root or:"
            fail "    sudo sh -c 'echo $USBFS_MIN_MB > $USBFS_FILE'"
            errors=$((errors+1))
        fi
    fi
fi

# --- 5. Device-node access -----------------------------------------------
step 5 "device-node access for $(id -un)"
node="/dev/bus/usb/${bus}/${devn}"
if [[ ! -e "$node" ]]; then
    warn "expected device node $node not found"
elif [[ -r "$node" && -w "$node" ]]; then
    ok "$node readable+writable"
else
    fail "$node not accessible to $(id -un). Either:"
    fail "    install rx888-tools' udev rules:  sudo cp <rx888-tools>/udev/99-rx888.rules /etc/udev/rules.d/ && sudo udevadm control --reload && sudo udevadm trigger"
    fail "    or run the demo with sudo:        sudo -E ..."
    errors=$((errors+1))
fi

echo
if [[ $errors -eq 0 ]]; then
    [[ $QUIET -eq 1 ]] || echo "preflight: OK"
    exit 0
else
    echo "preflight: $errors check(s) failed"
    exit 1
fi
