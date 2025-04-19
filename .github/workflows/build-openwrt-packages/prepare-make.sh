#!/usr/bin/env bash
set -eo pipefail

cd /home/user/openwrt

make -j"$(nproc)" toolchain/install
make -j"$(nproc)" target/linux/compile
make -j"$(nproc)" package/kernel/linux/compile
