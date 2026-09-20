#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: side placement line $LINENO" >&2' ERR
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
sleep .4
other=$(kad workspaceContext | jq -r --arg id "$main" '.applications[] | select(.windowId != $id) | .windowId')
if [[ ${KADUNCE_SIDE_TABLET:-0} == 1 ]]; then
    probe contactFocus
    kad showCardLine
    kad showActive
    sleep .3
fi
for kind in pointer touch; do
for target in '5 180' '1275 600' '1275 180' '5 600'; do
    home=$(probe windowGeometry "$main")
    read -r x y <<<"$(jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv' <<<"$home")"
    read -r tx ty <<<"$target"
    probe contactFocus
    probe pointer "$x" "$y"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 61 "$x" "$y"; fi
    sleep .12
    if [[ $kind == pointer ]]; then probe contactMotion "$tx" "$ty"; else probe motion 61 "$tx" "$ty"; fi
    state=$(kad nativeCarryState)
    jq -e '.carrying and .destinationPreview' <<<"$state"
    expected=$(jq -c '.destinationRect' <<<"$state")
    if [[ $ty == 180 ]]; then jq -e '.width > 700' <<<"$expected"; else jq -e '.width < 600' <<<"$expected"; fi
    if [[ $tx == 5 ]]; then jq -e '.x < 30' <<<"$expected"; else jq -e '.x > 350' <<<"$expected"; fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 61; fi
    sleep .55
    probe windowGeometry "$main" | jq -e --argjson target "$expected" '. == $target'
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
done
done
echo 'PASS: ordinary entry and repeated Bento side/share snaps match preview for mouse and touch'
for kind in pointer touch; do
    before=$(probe windowGeometry "$main")
    neighbor=$(probe windowGeometry "$other")
    read -r rx ry <<<"$(jq -nr --argjson a "$before" --argjson b "$neighbor" '[($a.x+$a.width+$b.x)/2,($a.y+$a.height*.15)] | map(floor) | @tsv')"
    probe pointer "$rx" "$ry"
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 71 "$rx" "$ry"; fi
    sleep .12
    if [[ $kind == pointer ]]; then probe contactMotion "$((rx+40))" "$ry"; else probe motion 71 "$((rx+40))" "$ry"; fi
    test "$(probe windowGeometry "$main")" = "$before"
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 71; fi
    sleep .6
    probe windowGeometry "$main" | jq -e --argjson old "$before" '.width > $old.width+20'
done
before=$(probe windowGeometry "$main")
neighbor=$(probe windowGeometry "$other")
read -r rx ry <<<"$(jq -nr --argjson a "$before" --argjson b "$neighbor" '[($a.x+$a.width+$b.x)/2,($a.y+$a.height/2)] | map(floor) | @tsv')"
probe down 72 "$rx" "$ry"
sleep .12
probe motion 72 "$((rx-100))" "$ry"
probe contactCancel
sleep .2
test "$(probe windowGeometry "$main")" = "$before"
echo 'PASS: pill rail previews without native resize, commits mouse/touch and cancels touch unchanged'
probe minimizeWindow "$other" true
sleep .6
probe windowGeometry "$main" | jq -e '.width > 1200'
probe minimizeWindow "$other" false
sleep .6
probe windowGeometry "$main" | jq -e '.width < 600 and .x < 30'
echo 'PASS: single card fills and restores requested side/share when companion returns'
if [[ ${KADUNCE_SIDE_TABLET:-0} != 1 ]]; then
    # CARD-LIFECYCLE.md §8 is growth-only. This launch asks for more width than
    # the larger pane of any two-pane shape fits, and the display is already at
    # its two-pane maximum, so no layout can grow to show it. It is refused and
    # left awake, and the panes the user placed do not move.
    resident_main=$(probe windowGeometry "$main")
    resident_other=$(probe windowGeometry "$other")
    panes_before=$(kad outputStageState | rg '^Virtual-0\|')
    client widerCompanion
    sleep .6
    incoming=$(kad workspaceContext | jq -r '.applications[] | select(.title == "Large admission probe") | .windowId')
    test -n "$incoming"
    test "$(probe windowMinimized "$incoming")" = false
    test "$(probe windowGeometry "$main")" = "$resident_main"
    test "$(probe windowGeometry "$other")" = "$resident_other"
    test "$(kad outputStageState | rg '^Virtual-0\|')" = "$panes_before"
    echo 'PASS: a launch the layout cannot grow for is refused awake and leaves the placed panes untouched'
fi
test "$(probe releaseRuntime)" = true
if [[ ${KADUNCE_SIDE_TABLET:-0} != 1 ]]; then
    sleep .6
    test "$(probe windowMinimized "$incoming")" = false
    for kind in pointer touch; do
        home=$(probe windowGeometry "$main")
        read -r x y <<<"$(jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv' <<<"$home")"
        probe contactFocus
        probe pointer "$x" "$y"
        client armMove
        if [[ $kind == pointer ]]; then probe contactButton true; else probe down 63 "$x" "$y"; fi
        sleep .12
        if [[ $kind == pointer ]]; then probe contactMotion 5 180; else probe motion 63 5 180; fi
        state=$(kad nativeCarryState)
        jq -e '.carrying and .destinationPreview' <<<"$state"
        expected=$(jq -c '.destinationRect' <<<"$state")
        if [[ $kind == pointer ]]; then probe contactButton false; else probe up 63; fi
        sleep .6
        probe windowGeometry "$main" | jq -e --argjson target "$expected" '. == $target'
        # CARD-LIFECYCLE.md §5: the resident the two-pane layout cannot keep
        # yields to card ownership awake. Nothing is parked, so it is never
        # minimized and release has nothing to wake.
        test "$(probe windowMinimized "$incoming")" = false
        test "$(probe windowMinimized "$other")" = false
        kad outputStageState | rg '^Virtual-0\|.*\|2$'
        test "$(probe releaseRuntime)" = true
        sleep .6
        test "$(probe windowMinimized "$incoming")" = false
    done
    echo 'PASS: an edge snap evicts the resident it cannot show as an awake card and keeps two panes'
fi
if [[ ${KADUNCE_SIDE_TABLET:-0} == 1 ]]; then
    probe pointer 500 350
    kad showCardLine
    sleep .35
    probe down 62 500 350
    sleep .4
    probe motion 62 1275 600
    state=$(kad nativeCarryState)
    jq -e '.lineCarrying and .destinationPreview' <<<"$state"
    expected=$(jq -c '.destinationRect' <<<"$state")
    jq -e '.x > 700 and .width < 600' <<<"$expected"
    probe up 62
    sleep .6
    a=$(probe windowGeometry "$main"); b=$(probe windowGeometry "$other")
    jq -en --argjson a "$a" --argjson b "$b" --argjson expected "$expected" '$a == $expected or $b == $expected'
    echo 'PASS: tablet Spread touch placement commits the same side/share preview'
    probe releaseRuntime
fi
if [[ ${KADUNCE_SIDE_TABLET:-0} != 1 ]]; then
    probe minimizeWindow "$incoming" true
    probe contactFocus
    kad toggleBentoOnOutput Virtual-0
    sleep .6
    a=$(probe windowGeometry "$main"); b=$(probe windowGeometry "$other")
    read -r rx ry <<<"$(jq -nr --argjson a "$a" --argjson b "$b" '[($a.x+$a.width+$b.x)/2,($a.y+$a.height/2)] | map(floor) | @tsv')"
    probe down 73 "$rx" "$ry"
    sleep .12
    probe motion 73 "$((rx+40))" "$ry"
fi
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if [[ ${KADUNCE_SIDE_TABLET:-0} != 1 ]]; then
    probe up 73
    echo 'PASS: unload during rail touch completes without delaying disable'
fi
