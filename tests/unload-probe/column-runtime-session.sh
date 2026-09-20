#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: column placement line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .6
probe contactObserve
test "$(probe contactStart)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
client companion
sleep .3
snap() {
    local home x y
    home=$(probe windowGeometry "$main")
    read -r x y <<<"$(jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv' <<<"$home")"
    probe contactFocus
    probe pointer "$x" "$y"
    client armMove
    probe contactButton true
    sleep .12
    probe contactMotion "$1" "$2"
    expected=$(kad nativeCarryState | jq -ec 'select(.carrying and .destinationPreview) | .destinationRect')
    probe contactButton false
    sleep .6
    probe windowGeometry "$main" | jq -e --argjson target "$expected" '. == $target'
}
snap 5 600
client companion
sleep .6
snap 1275 600
jq -e '.x > 800 and .y > 300 and .height < 450' <<<"$expected"
kad outputStageState | rg '^Virtual-0\|.*\|3$'
echo 'PASS: occupied column splits below, matching its reserved drop footprint'
before=$(probe windowGeometry "$main")
read -r x y <<<"$(jq -r '[.x+.width*.15,.y-7] | map(floor) | @tsv' <<<"$before")"
probe down 80 "$x" "$y"
probe motion 80 "$x" "$((y-40))"
probe up 80
test "$(probe windowGeometry "$main")" = "$before"
echo 'PASS: movement before rail hold does not resize'
probe down 81 "$x" "$y"
sleep .12
probe motion 81 "$x" "$((y-40))"
test "$(probe windowGeometry "$main")" = "$before"
probe up 81
kad nativeCarryState | jq -e '.bentoMotion | length >= 2'
sleep .6
kad nativeCarryState | jq -e '.bentoMotion | length == 0'
probe windowGeometry "$main" | jq -e --argjson before "$before" '.height > $before.height+20'
echo 'PASS: off-center horizontal divider touch resizes the split column'
probe releaseRuntime
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
