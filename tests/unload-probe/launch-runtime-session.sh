#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: new-app admission line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
QT_QPA_PLATFORM=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .5
probe contactObserve
probe contactStart
probe contactPrepareDecoration
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
original=$(probe windowGeometry "$main")
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
kad outputStageState | rg '^Virtual-0\|.*\|1\|0$'
client companion
sleep .5
kad outputStageState | rg '^Virtual-0\|.*\|2\|0$'
kad workspaceContext | jq -e '[.applications[] | select(.output=="Virtual-0" and (.minimized|not))] | length==2'
test "$(probe releaseRuntime)" = true
sleep .4
kad outputStageState | rg '^Virtual-0\|.*\|0\|0$'
probe windowGeometry "$main" | jq -e --argjson original "$original" '. == $original'
kad workspaceContext | jq -e '[.applications[] | select(.minimized)] | length==0'
echo 'PASS: new app expands single-card Bento to two visible members; release restores the original window'
