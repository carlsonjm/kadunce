#!/usr/bin/env bash
# CARD-LIFECYCLE.md §9: a Stack keeps its room in the row, so it moves with the
# row and nothing beside it moves when the row lands on it or leaves it. Four
# coloured cards, two of them a Stack, are photographed along a band through
# drags, landings and a drag begun during a landing, and the gap between the
# Stack and its neighbour must not change.
set -euo pipefail
trap 'echo "FAIL: stack still runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
source "$(dirname "${BASH_SOURCE[0]}")/spread-carry.bash"
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
declare -A tint=([Still A]=d03030 [Still B]=30a050 [Still C]=3050d0 [Still D]=d0a020)
for title in "Still A" "Still B" "Still C" "Still D"; do
    client colouredCompanion "$title" "${tint[$title]}" 560 420
    sleep .3
done
cards() { kad workspaceContext | jq '[.applications[] | select(.hasCard)] | length'; }
for attempt in {1..25}; do (( $(cards) == 5 )) && break; sleep .2; done
(( $(cards) == 5 ))
width=$(kad workspaceContext | jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width')
probe pointer 500 350
kad showCardLine
sleep .5
carry_onto_centre 61
probe up 61
sleep .8
kad workspaceContext | jq -e '[.applications[] | select(.hasCard and .stackSize == 2)] | length == 2'
stacked=$(kad workspaceContext | jq -r '[.applications[] | select(.hasCard and .stackSize == 2) | .title] | join(",")')
echo "stack $stacked"
chosen_stack() { kad workspaceContext | jq '[.applications[] | select(.hasCard and .selected)][0].stackSize'; }
# Rest on a lone card, the Stack beside it.
for key in 106 105 105; do
    [[ $(chosen_stack) == 1 ]] && break
    probe key "$key" 0
    sleep .5
done
[[ $(chosen_stack) == 1 ]]
samples=$(dirname "$XDG_RUNTIME_DIR")/still-samples.jsonl
: >"$samples"
bands() { python3 "$(dirname "$0")/capture-extents.py" 0 330 "$width" 40 d03030 30a050 3050d0 d0a020 | tee -a "$samples"; }
echo "rest $(bands)"
centre=$((width / 2))
# Drag the Stack to the middle and let the row settle on it.
probe down 70 "$centre" 350
sleep .15
for step in $(seq 1 30); do
    probe motion 70 $((centre - step * 20)) 350
    sleep .03
    (( step % 5 )) || echo "drag $((step * -20)) $(bands)"
done
probe up 70
for tick in 1 2 3 4 5 6 7 8; do
    sleep .05
    echo "settling $tick $(bands)"
done
sleep .8
echo "settled $(bands) chosen $(chosen_stack)"
# Back onto the lone card, the drag begun on it, and a new drag begun at once,
# while the Stack is still closing.
probe down 71 100 350
sleep .1
for step in $(seq 1 30); do probe motion 71 $((100 + step * 20)) 350; sleep .02; done
probe up 71
sleep .03
probe down 72 "$centre" 350
for step in 1 2 3 4 5 6 7 8; do
    probe motion 72 $((centre + step * 5)) 350
    sleep .03
    echo "redrag $((step * 5)) $(bands)"
done
sleep .4
echo "held $(bands)"
probe up 72
sleep 1
echo "after $(bands) chosen $(chosen_stack)"
stack_tints=$(IFS=,; for title in $stacked; do printf '%s,' "${tint[$title]}"; done)
python3 "$(dirname "$0")/still-gap-check.py" "$samples" "${stack_tints%,}" "$width"
echo "PASS: a Stack keeps its distance from its neighbour through drags and landings"
test "$(probe releaseRuntime)" = true
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
