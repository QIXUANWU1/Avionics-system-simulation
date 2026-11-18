#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
sha256sum -c --status config/layout-inputs.sha256
sha256sum -c --status third_party/genann/source-sha256.txt
flags=(-std=c11 -O2 -Wall -Wextra -Werror -I. -Iinclude -Iconfig -Ithird_party/genann -static)
gcc "${flags[@]}" src/linux/worker.c src/apps/advisory.c third_party/genann/genann.c core/sim.c -lm -o build/ima-worker.pending
gcc "${flags[@]}" src/linux/record_mirror.c -lm -o build/ima-record-mirror.pending
build/ima-worker.pending --check-layout
build/ima-record-mirror.pending --check-sources
for target in ima-worker ima-record-mirror; do mv "build/$target.pending" "build/$target"; done
sha256sum build/ima-worker build/ima-record-mirror config/generated_layout.h > build/sha256.txt
gcc --version > build/toolchain.txt
echo "Built two native Linux C binaries; validated both static layouts and mirror sources"
