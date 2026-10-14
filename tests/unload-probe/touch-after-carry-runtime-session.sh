#!/usr/bin/env bash
# A touch drag after a touch carry. Kadunce takes a window by its title bar to
# an edge and lets it go there; the next touch drag, by a different finger and
# with Kadunce switched off, must move a window with plain KWin. A move that
# KWin was made to give up while that first finger was still down leaves the
# second drag moving nothing.
set -euo pipefail
trap 'echo "FAIL: touch after carry line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
probe contactStart
probe contactPrepareDecoration
sleep .2
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
probe pointer 500 350
client armMove
probe down 62 500 350
sleep .15
probe motion 62 650 400
sleep .15
probe motion 62 5 400
sleep .15
kad nativeCarryState | jq -e '.carrying and .inputBusy and .destinationPreview'
probe up 62
sleep .3
kad nativeMoveTrace | jq -se 'any(.[]; .event == "drop-committed")'
kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
echo "PASS: the carry by touch landed at the edge"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .5
before=$(probe windowGeometry "$main")
echo "before second drag: $before"
x=$(jq -r '(.x + .width / 2) | floor' <<<"$before")
y=$(jq -r '(.y + .height / 2) | floor' <<<"$before")
probe pointer "$x" "$y"
client armMove
probe down 63 "$x" "$y"
sleep .15
probe motion 63 "$((x + 60))" "$((y + 40))"
sleep .15
probe motion 63 "$((x + 120))" "$((y + 80))"
sleep .15
after=$(probe windowGeometry "$main")
echo "during second drag: $after"
probe up 63
sleep .2
jq -e --argjson before "$before" '.x == $before.x + 120 and .y == $before.y + 80' <<<"$after"
echo "PASS: the next touch drag moves its window"
