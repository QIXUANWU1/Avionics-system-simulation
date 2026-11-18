#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
root="$PWD"
mkdir -p build/idl
sha256sum -c --status config/layout-inputs.sha256
sha256sum -c --status third_party/genann/source-sha256.txt
idlc="${IDLC:-idlc}"
if ! command -v "$idlc" >/dev/null; then
    printf '%s\n' "Cyclone DDS development files and idlc are required (Ubuntu: cyclonedds-dev cyclonedds-tools)." >&2
    exit 2
fi
dds_flags=()
if [[ -n "${DDS_INCLUDE_DIR:-}" ]]; then dds_flags+=("-I$DDS_INCLUDE_DIR" "-I$DDS_INCLUDE_DIR/$(gcc -dumpmachine)"); fi
if [[ -n "${DDS_LIB_DIR:-}" ]]; then
    dds_flags+=("-L$DDS_LIB_DIR" "-Wl,-rpath,$DDS_LIB_DIR" "-Wl,-rpath-link,$DDS_LIB_DIR")
    export LD_LIBRARY_PATH="$DDS_LIB_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
fi
(cd build/idl; "$idlc" "$root/dds/ImalabState.idl")
flags=(-std=c11 -O2 -Wall -Wextra -Werror -I. -Iinclude -Iconfig -Ithird_party/genann)
models=(src/apps/advisory.c third_party/genann/genann.c core/*.c)
gcc "${flags[@]}" "${dds_flags[@]}" -Ibuild/idl src/linux/worker.c src/linux/dds_link.c build/idl/ImalabState.c \
    src/comm/sample_route.c "${models[@]}" -lddsc -lm -o build/ima-worker.pending
gcc "${flags[@]}" -static src/linux/record_mirror.c -lm -o build/ima-record-mirror.pending
gcc "${flags[@]}" "${dds_flags[@]}" -Ibuild/idl src/linux/panel.c src/apps/cockpit.c src/apps/cabin_fixture.c \
    src/linux/dds_link.c build/idl/ImalabState.c "${models[@]}" -lddsc -lm -o build/ima-panel.pending
gcc "${flags[@]}" -static src/linux/health.c core/*.c -lm -o build/ima-health.pending
build/ima-worker.pending --check-layout
build/ima-record-mirror.pending --check-sources
for target in ima-worker ima-record-mirror ima-panel ima-health; do mv "build/$target.pending" "build/$target"; done
sha256sum build/ima-worker build/ima-record-mirror build/ima-panel build/ima-health config/generated_layout.h > build/sha256.txt
gcc --version > build/toolchain.txt
echo "Built DDS/UDP worker, offline mirror, C cockpit renderer, and trace health reader"
