#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
duration="${1:-30}"
image="${2:-ima-avionics-lab:local}"
if [[ ! "$duration" =~ ^([1-9]|[1-9][0-9]|1[0-7][0-9]|180)$ ]]; then
    echo "Usage: bash tools/preview_container.sh [seconds 1–180] [built image]" >&2
    exit 2
fi
command -v docker >/dev/null || { echo "Docker CLI is required." >&2; exit 2; }
docker image inspect "$image" >/dev/null 2>&1 || {
    echo "Build first: docker build -t $image ." >&2
    exit 2
}
prefix="ima-lab-$$-$RANDOM-$RANDOM"
network="$prefix"
volume="$prefix-frames"
web="$prefix-display"
panel="$prefix-panel"
containers=()
workers=()
network_created=0
volume_created=0
cleanup() {
    if (("${#containers[@]}")); then docker rm -f "${containers[@]}" >/dev/null 2>&1 || true; fi
    if ((volume_created)); then docker volume rm "$volume" >/dev/null 2>&1 || true; fi
    if ((network_created)); then docker network rm "$network" >/dev/null 2>&1 || true; fi
}
trap cleanup EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
docker network create "$network" >/dev/null
network_created=1
docker volume create "$volume" >/dev/null
volume_created=1

limits=(--read-only --cap-drop ALL --security-opt no-new-privileges --memory 128m --pids-limit 64)
containers+=("$web")
docker run -d --name "$web" "${limits[@]}" --network "$network" \
    -p 127.0.0.1::8080 -v "$volume:/frames" --entrypoint python3 "$image" /opt/ima/serve_panel.py >/dev/null
count=$(docker exec "$web" /ima-panel --domains 1)
[[ "$count" =~ ^[0-9]+$ ]] && ((count > 0 && count <= 32))
base=34000
epoch=$((1 + RANDOM * 65536 + RANDOM))
duration_ms=$((duration * 1000))
# All workers and the panel wait on one shared monotonic start time.
barrier='
left=90
while [ ! -s /frames/start ]; do
    [ "$left" -gt 0 ] || exit 2
    sleep 1
    left=$((left-1))
done
read -r start </frames/start
program=$1
shift
if [ "$program" = worker ]; then
    exec /ima-worker 1 "$1" "$2" "$3" "$start" "$4" 0 dds
fi
exec /ima-panel --live 1 "$1" "$2" "$start" "$3" /frames/panel.svg
'
for ((domain=0; domain<count; ++domain)); do
    worker="$prefix-domain-$domain"
    containers+=("$worker")
    workers+=("$worker")
    docker run -d --name "$worker" "${limits[@]}" --network "container:$web" \
        -v "$volume:/frames:ro" --entrypoint /bin/sh "$image" -ec "$barrier" ima \
        worker "$domain" "$base" "$epoch" "$duration_ms" >/dev/null
done
containers+=("$panel")
docker run -d --name "$panel" "${limits[@]}" --network "container:$web" \
    -v "$volume:/frames" --entrypoint /bin/sh "$image" -ec "$barrier" ima \
    panel "$base" "$epoch" "$duration_ms" >/dev/null
docker exec "$web" /bin/sh -ec '
    now=$(/ima-panel --clock-ns)
    printf "%s\n" "$((now+2200000000))" > /frames/.start
    mv /frames/.start /frames/start
'
address=$(docker port "$web" 8080/tcp)
printf '浏览器打开 http://%s/ ，仿真时长 %s 秒。\n' "$address" "$duration"
status=0
for name in "${workers[@]}" "$panel"; do
    if ! code=$(docker wait "$name") || [[ "$code" != 0 ]]; then
        docker logs --tail 12 "$name" >&2 || true
        status=1
    fi
done
mkdir -p build
out=$(mktemp -d "$PWD/build/container.XXXXXX")
cp tools/panel_viewer.html "$out/index.html"
if docker cp "$panel:/frames/panel.svg" "$out/panel.svg"; then
    printf '结束画面 %s/index.html\n' "$out"
else
    status=1
fi
exit "$status"
