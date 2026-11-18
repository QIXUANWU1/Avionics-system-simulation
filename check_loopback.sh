#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
test -x build/ima-worker && test -x build/ima-record-mirror
gcc -std=c11 -O2 -Wall -Wextra -Werror -I. -Iinclude -Iconfig tests/test_loopback.c -lm -o build/test-loopback
build/test-loopback
