#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
test -x build/ima-worker && test -x build/ima-record-mirror
gcc -std=c11 -O2 -Wall -Wextra -Werror -I. -Iinclude -Iconfig tests/test_loopback.c -lm -o build/test-loopback
build/test-loopback "${1:-dds}"
if [[ "${1:-dds}" = dds ]]; then
    build/ima-health 1 1643317310 build/loopback-dds.bin > build/health-dds.txt
    grep 'coverage=progress-seen' build/health-dds.txt
fi
