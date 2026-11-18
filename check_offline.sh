#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
gcc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -I. -Iinclude tests/test_sampling.c -lm -o build/test-sampling
build/test-sampling
gcc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -I. -Iinclude -Ithird_party/genann tests/test_advisory.c src/apps/advisory.c third_party/genann/genann.c core/sim.c -lm -o build/test-advisory
build/test-advisory
