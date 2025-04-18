#!/usr/bin/env bash
set -eo pipefail

# build-openwrt-packages-commands.sh
# Wird im Container ausgeführt. ENV-Variablen OPENWRT_VERSION und GIT_TAG sind gesetzt.

# 1) OpenWRT sources klonen
git clone --branch v"$OPENWRT_VERSION" https://git.openwrt.org/openwrt/openwrt.git /home/user/openwrt
# 2) Paket-Repo klonen
git clone --branch "$GIT_TAG" https://github.com/Zerogiven-OpenWRT-Packages/Quectel-RM520N-Thermal.git /home/user/openwrt/package/quectel-rm520n-thermal
cd /home/user/openwrt

# 3) Feeds aktualisieren und installieren
./scripts/feeds update -a
./scripts/feeds install -a

# 4) Standard-Konfiguration herunterladen
wget "https://mirror-03.infra.openwrt.org/releases/$OPENWRT_VERSION/targets/mediatek/filogic/config.buildinfo" -O /home/user/openwrt/.config
make defconfig

# 5) Quectel-Paket aktivieren
sed -i 's|^# CONFIG_PACKAGE_quectel-rm520n-thermal is not set$|CONFIG_PACKAGE_quectel-rm520n-thermal=m|' /home/user/openwrt/.config

# 6) Toolchain installieren und Kernel/Packages kompilieren
make -j"$(nproc)" toolchain/install
make -j"$(nproc)" target/linux/compile
make -j"$(nproc)" package/kernel/linux/compile

if [ $DEBUG_MAKE -eq 1 ]; then
  make -j"$(nproc)" V=sc package/quectel-rm520n-thermal/compile
else
  make -j"$(nproc)" package/quectel-rm520n-thermal/compile
fi

# 7) Artefakte finden
KMOD_FILE=$(find /home/user/openwrt/bin -type f -name "kmod-quectel-rm520n-thermal*.ipk" | head -n1)
DAEMON_FILE=$(find /home/user/openwrt/bin -type f -name "quectel-rm520n-thermal*.ipk" | head -n1)
if [[ -z "$KMOD_FILE" || -z "$DAEMON_FILE" ]]; then
  echo "Fehler: IPK-Dateien nicht gefunden!" >&2
  exit 1
fi

# 8) Artefakte in Version-Ordner kopieren
SHORT_VER="$(echo "$OPENWRT_VERSION" | awk -F. '{print $1 "." $2}')"
mkdir -p /openwrt-bin/"$SHORT_VER"
cp "$KMOD_FILE" /openwrt-bin/"$SHORT_VER"/
cp "$DAEMON_FILE" /openwrt-bin/"$SHORT_VER"/
