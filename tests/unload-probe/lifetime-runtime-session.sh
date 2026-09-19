#!/usr/bin/env bash
set -euo pipefail
# Disposable fixture only: Virtual-0 tablet predicate and direct system edges.
# Neither override belongs in the production plugin.
trap 'echo "FAIL: lifetime line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe pointer 500 350
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
original=$(probe windowGeometry "$main")
kad showCardLine
kad showActive
sleep .3
active=$(probe windowGeometry "$main")
for cycle in 1 2 3; do
    kad showCardLine
    sleep .2
    test "$(probe windowGeometry "$main")" = "$active"
    kad showActive
    sleep .2
    test "$(probe windowGeometry "$main")" = "$active"
done
echo 'PASS: repeated Active/Spread keeps native geometry'
# A native resize request must not discard managed ownership or geometry.
client armResize
probe down 71 500 600
sleep .15
probe motion 71 500 550
probe up 71
sleep .2
kad workspaceContext | jq -e '.cardStage.presentation == "active"'
test "$(probe windowGeometry "$main")" = "$active"
echo 'PASS: Active resize rejected without ownership release'
probe down 72 640 775
probe motion 72 640 755
probe motion 72 640 735
probe motion 72 640 680
probe up 72
sleep .3
kad workspaceContext | jq -e '.cardStage.presentation == "spread"'
test "$(probe windowGeometry "$main")" = "$active"
echo 'PASS: bottom edge swipe enters Spread without native resize'
kad showActive
# A second Active app must not overwrite the first app restore record.
client companion
sleep .7
test "$(probe windowGeometry "$main")" = "$active"
kad showCardLine
sleep .2
test "$(probe windowGeometry "$main")" = "$active"
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
test "$(probe windowGeometry "$main")" = "$original"
echo 'PASS: retained app record survives second app and Bento round trip'
kad showCardLine
kad showActive
kad showCardLine
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
test "$(probe windowGeometry "$main")" = "$original"
echo 'PASS: unload from Spread restores original desktop geometry'
