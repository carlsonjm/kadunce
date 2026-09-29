#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: X11 entry line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
QT_QPA_PLATFORM=xcb "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
# watch returns whether an xdg protocol exists; X11 must not have one.
test "$(probe contactStart)" = false
test "$(probe contactPrepareDecoration)" = true
sleep .4
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
original=$(probe windowGeometry "$main")
for kind in pointer touch; do
    bounds=$(probe contactState)
    x=$(jq '.titleX|floor' <<<"$bounds"); y=$(jq '.titleY|floor' <<<"$bounds")
    probe pointer "$x" "$y"
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 62 "$x" "$y"; fi
    if [[ $kind == pointer ]]; then probe contactMotion "$((x+50))" "$((y+50))"; else probe motion 62 "$((x+50))" "$((y+50))"; fi
    sleep .15
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
    if [[ $kind == pointer ]]; then probe contactMotion 2555 350; else probe motion 62 2555 350; fi
    kad nativeCarryState | jq -e '.carrying and .inputBusy and .destinationPreview'
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 62; fi
    sleep .3
    kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name=="Virtual-1" and .bentoActive)]|length==1'
    test "$(kad toggleBentoOnOutput Virtual-1)" = true
    sleep .3
    probe windowGeometry "$main" | jq -e --argjson original "$original" '. == $original'
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: Xwayland real decoration pointer/touch carry, external edge Bento and restoration'
