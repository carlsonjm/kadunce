#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: membership line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
full=$(probe windowGeometry "$main")
client companion
sleep .5
second=$(kad workspaceContext | jq -r --arg id "$main" '.applications[] | select(.windowId != $id) | .windowId')
for id in "$main" "$second"; do
    survivor=$main; [[ $id != "$main" ]] || survivor=$second
    test "$(probe minimizeWindow "$id" true)" = true
    sleep .6
    kad outputStageState | rg '^Virtual-0\|.*\|1$'
    test "$(probe windowGeometry "$survivor")" = "$full"
    kad workspaceContext | jq -e --arg id "$id" '.applications[] | select(.windowId == $id) | .minimized'
    test "$(probe minimizeWindow "$id" false)" = true
    sleep .6
    kad outputStageState | rg '^Virtual-0\|.*\|2$'
done
echo 'PASS: either minimized pane yields full space; restore rejoins'
probe minimizeWindow "$main" true
probe minimizeWindow "$second" true
sleep .6
kad outputStageState | rg '^Virtual-0\|.*\|0$'
kad workspaceContext | jq -e '[.applications[] | select(.minimized)] | length == 2'
probe minimizeWindow "$main" false
sleep .6
test "$(probe windowGeometry "$main")" = "$full"
echo 'PASS: empty Bento retains restore membership and accepts restoration'
client companion
sleep .6
kad outputStageState | rg '^Virtual-0\|.*\|2$'
kad workspaceContext | jq -e --arg id "$second" '.applications[] | select(.windowId == $id) | .minimized'
echo 'PASS: new launch joins without resurrecting user-minimized card'
client companion
sleep .6
# CARD-LIFECYCLE.md §8: the layout is already at its cap, so the third launch
# is refused rather than parked. It stays awake and the two panes stay put.
kad outputStageState | rg '^Virtual-0\|.*\|2$'
third=$(kad workspaceContext | jq -r '[.applications[] | select(.minimized|not)][-1].windowId')
test "$(probe windowMinimized "$third")" = false
visible=$(kad workspaceContext | jq -r '[.applications[] | select(.minimized|not)][0].windowId')
probe minimizeWindow "$visible" true
sleep .6
kad outputStageState | rg '^Virtual-0\|.*\|[12]$'
test "$(probe windowMinimized "$second")" = true
test "$(probe windowMinimized "$visible")" = true
echo 'PASS: a launch the layout cannot show stays awake, and a vacancy does not reopen user-minimized cards'
test "$(probe releaseRuntime)" = true
sleep .6
kad_state=$(probe windowGeometry "$second")
test "$kad_state" != '{}'
test "$(probe windowMinimized "$second")" = true
test "$(probe windowMinimized "$visible")" = true
echo 'PASS: explicit release preserves user minimization'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
test "$(probe windowGeometry "$main")" != '{}'
echo 'PASS: effect unload retains live windows'
