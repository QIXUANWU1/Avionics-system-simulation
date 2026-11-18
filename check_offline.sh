#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
mkdir -p build
gcc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -I. -Iinclude tests/test_sampling.c -lm -o build/test-sampling
build/test-sampling
gcc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -I. -Iinclude -Ithird_party/genann tests/test_advisory.c src/apps/advisory.c third_party/genann/genann.c core/*.c -lm -o build/test-advisory
build/test-advisory
gcc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -I. -Iinclude -Icore -Iconfig tests/test_aircraft.c src/comm/sample_route.c core/*.c -lm -o build/test-aircraft
build/test-aircraft
gcc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -I. -Icore tests/test_airdata.c core/*.c -lm -o build/test-airdata
build/test-airdata
gcc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -fno-sanitize-recover=all -I. -Icore tests/test_extended.c core/*.c -lm -o build/test-extended
build/test-extended
gcc -std=c11 -O2 -Wall -Wextra -Werror -I. -Iinclude tests/test_health.c -o build/test-health
build/test-health
