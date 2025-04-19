#!/usr/bin/env bash
set -eo pipefail

make -j"$(nproc)" toolchain/install
make -j"$(nproc)" target/linux/compile
make -j"$(nproc)" package/kernel/linux/compile
