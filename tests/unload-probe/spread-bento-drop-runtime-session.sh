#!/usr/bin/env bash
# CARD-LIFECYCLE.md §5, §6 and §9, INPUT.md § Spread: in Spread the Bento group
# is a picture of its layout, and a card held over it names the pane under the
# finger. That pane gives way to a cutout and the card slides under the group in
# its place, showing through it. Let go, the card becomes that pane and the
# group opens as its layout; the pane it replaces becomes a card of its own; the
# pane on the other half keeps its window and its side, and the row holds still
# while the finger moves between the panes.
#
# The tablet holds a Bento pair beside one individual card. Spread opens on the
# group, and the card beside it is held and let go over the group's left half,
# then, from the same start, over its right half, each by touch and by pointer.
# Each pass photographs the strip just above the group while the card is held,
# where nothing rises now, and the middle of the pane that gives way, where the
# held card shows through.
#
# Needs the tablet fixture: only a display that can own cards presents Spread.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: spread bento drop: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
frame() { probe windowGeometry "$1"; }
twoPanes() { kad outputStageState | grep -q '^Virtual-0|.*|2$'; }
# The first window's frame ends where the second's begins, or before.
leftOf() { test "$(frame "$1" | jq '.x + .width')" -le "$(frame "$2" | jq '.x')"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, hasCard, entry, stackId, stackSize, selected, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c '[.[] | select(.normal) | {caption, minimized, x, y, width, height}]')"
}
# The Bento group in Spread is the entry whose members share one stack.
member() { context --arg a "$1" --arg b "$2" '[.applications[] | select(.windowId == $a or .windowId == $b)]
    | length == 2 and all(.hasCard and .stackSize == 2) and (map(.stackId) | unique | length == 1)'; }
alone() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .stackSize == 1 and (.minimized | not)'; }
entryOf() { kad workspaceContext | jq -r --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .entry'; }
shots=$(dirname "$XDG_RUNTIME_DIR")
# The mean red, green and blue of a small square of the display.
colour() {
    python3 "$(dirname "$0")/capture-png.py" "$1" "$2" 24 24 "$shots/$3.png"
    python3 -c 'import sys; from PIL import Image; im = Image.open(sys.argv[1]).convert("RGB"); px = list(im.getdata()); print(*(sum(p[i] for p in px) // len(px) for i in range(3)))' "$shots/$3.png"
}
near() { read -r a b c <<<"$1"; read -r x y z <<<"$2"; (( (a - x) ** 2 + (b - y) ** 2 + (c - z) ** 2 < 900 )); }
apart() { ! near "$1" "$2"; }
press() { if [[ $kind == pointer ]]; then probe pointer "$1" "$2"; probe contactButton true; else probe down "$touch" "$1" "$2"; fi; }
drag() { if [[ $kind == pointer ]]; then probe contactMotion "$1" "$2"; else probe motion "$touch" "$1" "$2"; fi; }
lift() { if [[ $kind == pointer ]]; then probe contactButton false; else probe up "$touch"; fi; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
touch=60
for pass in left:touch right:pointer right:touch left:pointer; do
    half=${pass%:*} kind=${pass#*:} touch=$((touch + 1)) name=${pass/:/-}
    # KWin opens a window on the display under the pointer.
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    client colouredCompanion 'Drop Two' 2e8b57 560 420
    client colouredCompanion 'Drop Three' c88a1e 560 420
    sleep .8
    probe contactObserve
    probe contactStart >/dev/null
    probe contactFocus >/dev/null
    sleep .2
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    # §3's Bento action pairs the Active card, on the left, with its neighbour.
    test "$(kad toggleBentoOnOutput Virtual-0)" = true
    sleep 1
    kad showCardLine
    sleep 1
    report "$name-spread"
    read -r first second < <(kad workspaceContext | jq -r '[.applications[] | select(.hasCard and .stackSize == 2) | .windowId] | "\(.[0]) \(.[1])"')
    held=$(kad workspaceContext | jq -r 'first(.applications[] | select(.hasCard and .stackSize == 1)) | .windowId')
    if [[ $(frame "$first" | jq '.x') -lt $(frame "$second" | jq '.x') ]]; then left=$first right=$second; else left=$second right=$first; fi
    if [[ $half == left ]]; then replaced=$left stays=$right; else replaced=$right stays=$left; fi
    echo "$name: left pane $left right pane $right held card $held"
    check "$name: Spread shows the pair as one group beside one card" member "$left" "$right"
    check "$name: the group is the centred entry" context '[.applications[] | select(.hasCard and .stackSize == 2 and .selected)] | length == 1'

    # Hold the card beside the group, then rest it with the finger over the
    # middle of one half of the group, where the group stands while it is held.
    context=$(kad workspaceContext)
    width=$(jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width | floor' <<<"$context")
    groupRect() { kad workspaceContext | jq -r '[.applications[] | select(.hasCard and .stackSize == 2 and .spreadRect)][0].spreadRect
        | "\(.x | floor) \(.y | floor) \(.width | floor) \(.height | floor)"'; }
    read -r gx gy gw gh < <(groupRect)
    pick=$(jq -r --arg id "$held" --argjson w "$width" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | (([.x, 0] | max) + ([.x + .width, $w] | min)) / 2 | floor' <<<"$context")
    y=$((gy + gh / 2))
    heldColour=$(colour "$((pick - 12))" "$((y - 12))" "$name-held-card")
    # The card stands in the Active card's place, as every awake card does.
    place=$(frame "$held" | jq -c .)
    press "$pick" "$y"
    sleep .4
    finger=$((pick < gx ? pick + 30 : pick - 30))
    drag "$finger" "$y"
    sleep .5
    read -r gx gy gw gh < <(groupRect)
    # Each pane's middle, as the group draws it: the left pane holds the
    # left 61 per cent of the layout, the right pane the rest.
    leftMiddle=$((gx + gw * 30 / 100)) rightMiddle=$((gx + gw * 81 / 100))
    if [[ $half == left ]]; then target=$leftMiddle other=$rightMiddle; else target=$rightMiddle other=$leftMiddle; fi
    strip=$((gy - 30))
    # Nothing stands above the group's far right corner: the strip there is
    # what the display shows behind the row.
    behind=$(colour "$((width - 40))" "$strip" "$name-behind")
    echo "$name: card picked at $pick $y; held, the group is at $gx $gy ${gw}x$gh; finger to $target; behind the row $behind"
    for step in 1 2 3 4 5 6; do
        drag "$((finger + (target - finger) * step / 6))" "$y"
        sleep .05
    done
    sleep .3
    read -r arrivedX _ < <(groupRect)
    sleep .7
    carry=$(kad nativeCarryState)
    echo "$name: over the group $(jq -c '{lineCarrying, carryAim, carryIndex, carryPane, lineRect}' <<<"$carry")"
    check "$name: the pane under the finger is the one it would replace" \
        jq -e --arg id "$replaced" '.carryAim == "pane" and .carryPane == $id' <<<"$carry"
    read -r restedX _ < <(groupRect)
    check "$name: the row holds still under the finger" test "$arrivedX" = "$restedX"
    risen=$(colour "$((target - 12))" "$strip" "$name-target-held")
    otherHeld=$(colour "$((other - 12))" "$strip" "$name-other-held")
    through=$(colour "$((target - 12))" "$((y - 12))" "$name-through-cutout")
    # Beside the seam, inside the other pane: a card on top under the finger
    # would cover it; a card under the group, in its pane's shape, does not.
    if [[ $half == left ]]; then seam=$((gx + gw * 66 / 100)); else seam=$((gx + gw * 55 / 100)); fi
    otherPane=$(colour "$((other - 12))" "$((y - 12))" "$name-other-pane")
    # The whole group as it looks with the card under it, kept for review.
    python3 "$(dirname "$0")/capture-png.py" "$((gx > 60 ? gx - 60 : 0))" "$((gy > 40 ? gy - 40 : 0))" \
        "$((gw + 120))" "$((gh + 80))" "$shots/$name-tucked.png"
    beside=$(colour "$((seam - 12))" "$((y - 12))" "$name-beside-seam")
    echo "$name: strip while held $risen / $otherHeld; through the cutout $through; the card $heldColour; the other pane $otherPane, beside the seam $beside"
    check "$name: nothing rises out of the group" near "$risen" "$behind"
    check "$name: the other pane stays where it is" near "$otherHeld" "$behind"
    check "$name: the card shows through where the pane was" near "$through" "$heldColour"
    check "$name: the card is under the group, not over the other pane" near "$beside" "$otherPane"
    check "$name: the other pane is not the card" apart "$otherPane" "$heldColour"
    lift
    # Let go, the card grows into its pane from where it was let go: it may
    # wait there a moment for its client, then moves the whole way.
    arrivals=()
    for sample in {1..20}; do
        arrivals+=("$(kad nativeCarryState | jq -c --arg id "$held" \
            '[.bentoMotion[] | select(.window == $id and .arrival)][0] // empty | {started, rect, target}')")
        sleep .03
    done
    printf '%s\n' "${arrivals[@]}" | grep . | sed "s/^/$name: arrival /"
    check "$name: the card grows into its pane from where it was let go" \
        grep -q started < <(printf '%s\n' "${arrivals[@]}")
    check "$name: it is drawn moving, not set down in its pane" \
        grep -q '"started":true' < <(printf '%s\n' "${arrivals[@]}")
    # Let go, the group opens as its layout with the card in that pane.
    sleep .6
    report "$name-presented"
    echo "$name: held card at $(frame "$held" | jq -c '{x, width}'), staying pane at $(frame "$stays" | jq -c '{x, width}'), replaced pane at $(frame "$replaced" | jq -c '{x, width}')"
    check "$name: the layout holds two panes" twoPanes
    check "$name: the card is a pane of it, not a card" \
        context --arg id "$held" '.cardStage.presentation == "bento"
            and ([.applications[] | select(.windowId == $id and .hasCard)] | length == 0)'
    if [[ $half == left ]]; then
        check "$name: the card stands wholly left of the other pane" leftOf "$held" "$stays"
    else
        check "$name: the card stands wholly right of the other pane" leftOf "$stays" "$held"
    fi
    check "$name: the other pane keeps its side" \
        test "$(frame "$stays" | jq --argjson w "$width" '.x < $w / 2')" = "$([[ $half == right ]] && echo true || echo false)"
    check "$name: the replaced pane is a card of its own" alone "$replaced"
    echo "$name: replaced pane at $(frame "$replaced" | jq -c .), the Active card's place $place"
    check "$name: the replaced pane stands in the Active card's place" \
        test "$(frame "$replaced" | jq -c .)" = "$place"

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    probe contactDrop
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: spread bento drop: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card let go on a pane of the Bento group in Spread replaces that pane, by touch and by pointer'
