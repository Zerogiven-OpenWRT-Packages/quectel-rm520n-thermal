#!/usr/bin/env bash
set -eo pipefail

sudo chown -R user:user /home/user/openwrt

cd /home/user/openwrt

make -j"$(nproc)" toolchain/install
make -j"$(nproc)" target/linux/compile
make -j"$(nproc)" package/kernel/linux/compile
