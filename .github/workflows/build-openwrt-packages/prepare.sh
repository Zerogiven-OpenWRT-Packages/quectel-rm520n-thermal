#!/usr/bin/env bash
set -eo pipefail

sudo chown -R user:user /home/user/openwrt

cd /home/user/openwrt

./scripts/feeds update -a
./scripts/feeds install -a

wget "https://mirror-03.infra.openwrt.org/releases/$OPENWRT_VERSION/targets/mediatek/filogic/config.buildinfo" -O /home/user/openwrt/.config

make defconfig

sed -i 's|^# CONFIG_PACKAGE_quectel-rm520n-thermal is not set$|CONFIG_PACKAGE_quectel-rm520n-thermal=m|' /home/user/openwrt/.config
