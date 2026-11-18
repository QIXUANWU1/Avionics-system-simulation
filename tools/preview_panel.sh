#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
duration="${1:-30}"
[[ "$duration" =~ ^([1-9]|[1-9][0-9]|1[0-7][0-9]|180)$ ]] || {
    echo "Usage: bash tools/preview_panel.sh [seconds 1–180]" >&2; exit 2;
}
for program in ima-worker ima-panel; do
    [[ -x "build/$program" ]] || { echo "Run bash build.sh first." >&2; exit 2; }
done
out="$(mktemp -d "$PWD/build/preview.XXXXXX")"
cp tools/panel_viewer.html "$out/index.html"
base=$((34000 + $$ % 24000))
session=$((1 + (RANDOM << 16) + RANDOM))
start=$(build/ima-panel --clock-ns)
start=$((start + 1800000000))
pids=()
cleanup() {
    for pid in "${pids[@]}"; do kill "$pid" 2>/dev/null || true; done
    wait || true
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
# Resolve the layout size from the same generated C configuration as the worker.
count=$(build/ima-panel --domains 1)
for ((domain=0; domain<count; ++domain)); do
    build/ima-worker 1 "$domain" "$base" "$session" "$start" "$((duration * 1000))" 0 dds \
        >"$out/worker-$domain.log" 2>&1 &
    pids+=("$!")
done
printf 'Open in a local browser: %s/index.html\n' "$out"
if command -v wslpath >/dev/null; then
    wslpath -w "$out/index.html"
fi
build/ima-panel --live 1 "$base" "$session" "$start" "$((duration * 1000))" "$out/panel.svg"
status=0
for pid in "${pids[@]}"; do wait "$pid" || status=1; done
pids=()
if ((status)); then echo "A worker exited with an error; see $out/worker-*.log." >&2; fi
exit "$status"
