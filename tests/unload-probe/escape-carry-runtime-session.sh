#!/usr/bin/env bash
# INPUT.md: Escape while dragging a window cancels the drag and the window
# stays as it was, including a carry Kadunce has taken at an edge, with its
# landing already shown. It glides home from under the hand, and letting go
# afterwards places nothing.
set -euo pipefail
trap 'echo "FAIL: escape carry runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
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
for kind in pointer touch; do
for target in '5 180' '5 600'; do
    home=$(probe windowGeometry "$main")
    read -r x y <<<"$(jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv' <<<"$home")"
    read -r tx ty <<<"$target"
    probe contactFocus
    probe pointer "$x" "$y"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 81 "$x" "$y"; fi
    sleep .12
    if [[ $kind == pointer ]]; then probe contactMotion "$tx" "$ty"; else probe motion 81 "$tx" "$ty"; fi
    kad nativeCarryState | jq -e '.carrying and .destinationPreview'
    probe key 1 0
    # §10 Cancel: the window glides back from where it was drawn under the
    # hand, rather than vanishing there and reappearing at home.
    back=$(kad nativeCarryState)
    echo "RESULT escape-glide $kind $target $(jq -c '{dropSettling, dropWindow, dropRect, dropTarget}' <<<"$back")"
    jq -e --arg id "$main" --argjson home "$home" \
        '.dropSettling and .dropWindow == $id and .dropRect.x < $home.x' <<<"$back"
    sleep .3
    kad nativeCarryState | jq -e '(.carrying|not) and (.destinationPreview|not)'
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 81; fi
    sleep .55
    probe windowGeometry "$main" | jq -e --argjson home "$home" '. == $home'
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
done
done
echo 'PASS: Escape cancels an edge carry with its landing shown, by mouse or touch; the window stays as it was'
