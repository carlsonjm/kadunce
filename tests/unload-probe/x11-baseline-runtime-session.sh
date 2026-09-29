#!/usr/bin/env bash
set -euo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
KADUNCE_TEST_FRAMELESS=1 QT_QPA_PLATFORM=xcb "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
test "$(probe contactStart)" = false
# Kadunce is deliberately never loaded: native KWin start and cancel only.
for kind in pointer touch; do
    probe pointer 500 350
    client reset
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 62 500 350; fi
    sleep .2
    probe contactState
    probe contactEndMove
    sleep .2
    before=$(client state)
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 62; fi
    sleep .2
    after=$(client state)
    echo "NATIVE BASELINE $kind $before $after"
done
echo 'PASS: native-only Xwayland cancellation baseline recorded'
