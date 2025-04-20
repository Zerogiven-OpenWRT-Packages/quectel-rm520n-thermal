#!/usr/bin/env bash
# set -eo pipefail

SHORT_VER="$(echo "24.10.0" | awk -F. '{print $1 "." $2}')"
mkdir -p /openwrt-bin/"$SHORT_VER"
touch /openwrt-bin/"$SHORT_VER"/1.txt
touch /openwrt-bin/"$SHORT_VER"/2.txt

# cp -v "$KMOD_FILE" /openwrt-bin/"$SHORT_VER"/
# cp -v "$DAEMON_FILE" /openwrt-bin/"$SHORT_VER"/