#!/bin/sh
set -eu
binary=${1:?Informe o caminho do compositor}
runtime=$(mktemp -d)
pid=
cleanup() {
    if [ -n "$pid" ]; then kill "$pid" 2>/dev/null || true; wait "$pid" 2>/dev/null || true; fi
    rm -rf "$runtime"
}
trap cleanup EXIT HUP INT TERM
chmod 700 "$runtime"
XDG_RUNTIME_DIR="$runtime" WLR_BACKENDS=headless WLR_RENDERER=pixman WLR_HEADLESS_OUTPUTS=1 \
    "$binary" >"$runtime/log" 2>&1 &
pid=$!
attempt=0
while ! grep -q 'JLinux compositor pronto:' "$runtime/log"; do
    if ! kill -0 "$pid" 2>/dev/null || [ "$attempt" -ge 40 ]; then
        cat "$runtime/log"
        exit 1
    fi
    attempt=$((attempt + 1))
    sleep 0.1
done
grep -q 'Saída ativa:' "$runtime/log"
socket=$(sed -n 's/.*WAYLAND_DISPLAY=//p' "$runtime/log" | tail -n 1)
[ -S "$runtime/$socket" ]
kill -TERM "$pid"
if ! wait "$pid"; then
    cat "$runtime/log"
    exit 1
fi
pid=
[ ! -e "$runtime/$socket" ]
cat "$runtime/log"
