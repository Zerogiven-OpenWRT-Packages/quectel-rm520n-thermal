#!/usr/bin/env bash
set -eo pipefail

if [ "${DEBUG_MAKE:-0}" -eq 1 ]; then
  make -j"$(nproc)" V=sc package/quectel-rm520n-thermal/compile
else
  make -j"$(nproc)" package/quectel-rm520n-thermal/compile
fi

KMOD_FILE=$(find /home/user/openwrt/bin -type f -name "quectel-rm520n-thermal*.ipk" | grep kmod | head -n1)
DAEMON_FILE=$(find /home/user/openwrt/bin -type f -name "quectel-rm520n-thermal*.ipk" | grep -v kmod | head -n1)
if [[ -z "$KMOD_FILE" || -z "$DAEMON_FILE" ]]; then
  echo "Fehler: IPK-Dateien nicht gefunden!" >&2
  exit 1
fi

SHORT_VER="$(echo "$OPENWRT_VERSION" | awk -F. '{print $1 "." $2}')"
mkdir -p /openwrt-bin/"$SHORT_VER"
cp "$KMOD_FILE" /openwrt-bin/"$SHORT_VER"/
cp "$DAEMON_FILE" /openwrt-bin/"$SHORT_VER"/
