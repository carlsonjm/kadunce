#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: local runtime line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
test "$(probe contactStart)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
client companion
sleep .3
other=$(kad workspaceContext | jq -r --arg main "$main" '.applications[] | select(.windowId != $main) | .windowId')
original_main=$(probe windowGeometry "$main")
original_other=$(probe windowGeometry "$other")
probe contactFocus
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
for kind in pointer touch; do
for scenario in swap return dock; do
    home=$(probe windowGeometry "$main")
    neighbor=$(probe windowGeometry "$other")
    read -r x y <<<"$(jq -r '[.x + .width/2, .y + .height/2] | map(floor) | @tsv' <<<"$home")"
    read -r tx ty <<<"$(jq -r '[.x + .width/2, .y + .height/2] | map(floor) | @tsv' <<<"$neighbor")"
    probe contactFocus
    probe pointer "$x" "$y"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 51 "$x" "$y"; fi
    sleep .1
    kad nativeCarryState | jq -e '.carrying and .inputBusy'
    if [[ $scenario == return ]]; then
        if [[ $kind == pointer ]]; then probe contactMotion 1800 350; else probe motion 51 1800 350; fi
        kad nativeCarryState | jq -e '.destinationPreview'
        tx=$x; ty=$((y + 30))
    elif [[ $scenario == dock ]]; then
        # Below the work area, but not in the newly approved bottom-edge exit.
        ty=760
    fi
    if [[ $kind == pointer ]]; then probe contactMotion "$tx" "$ty"; else probe motion 51 "$tx" "$ty"; fi
    state=$(kad nativeCarryState)
    if [[ $scenario == dock ]]; then
        jq -e '(.destination|not) and (.destinationPreview|not)' <<<"$state"
    else
        expected=$home
        if [[ $scenario == swap ]]; then expected=$neighbor; fi
        jq -e --argjson target "$expected" '.destinationPreview and .destinationRect == $target' <<<"$state"
    fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 51; fi
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
    sleep .3
    expected=$home; displaced=$neighbor
    if [[ $scenario == swap ]]; then expected=$neighbor; displaced=$home; fi
    probe windowGeometry "$main" | jq -e --argjson target "$expected" '. == $target'
    probe windowGeometry "$other" | jq -e --argjson target "$displaced" '. == $target'
done
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .4
probe windowGeometry "$main" | jq -e --argjson target "$original_main" '. == $target'
probe windowGeometry "$other" | jq -e --argjson target "$original_other" '. == $target'
echo 'PASS: same-output Bento pointer/touch pane exchange, external-preview return, dock exclusion and original restoration'
