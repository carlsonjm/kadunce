#!/usr/bin/env bash
# CARD-LIFECYCLE.md §5 and §6: in Spread the Bento group is a picture of its
# layout, and a card held there and let go on one half of it replaces the pane
# on that half. The card becomes that pane; the pane it replaces becomes a card
# of its own in Spread; the pane on the other half keeps its window and its
# side. How the target looks while the card is held is not checked here.
#
# The tablet holds a Bento pair beside one individual card. Spread opens on the
# group, and the card beside it is held and let go over the group's left half,
# then, from the same start, over its right half.
#
# Needs the tablet fixture: only a display that can own cards presents Spread.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
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
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for half in left right; do
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
    report "$half-spread"
    read -r first second < <(kad workspaceContext | jq -r '[.applications[] | select(.hasCard and .stackSize == 2) | .windowId] | "\(.[0]) \(.[1])"')
    held=$(kad workspaceContext | jq -r 'first(.applications[] | select(.hasCard and .stackSize == 1)) | .windowId')
    if [[ $(frame "$first" | jq '.x') -lt $(frame "$second" | jq '.x') ]]; then left=$first right=$second; else left=$second right=$first; fi
    if [[ $half == left ]]; then replaced=$left stays=$right; else replaced=$right stays=$left; fi
    echo "$half: left pane $left right pane $right held card $held"
    check "$half: Spread shows the pair as one group beside one card" member "$left" "$right"
    check "$half: the group is the centred entry" context '[.applications[] | select(.hasCard and .stackSize == 2 and .selected)] | length == 1'

    # Hold the card beside the group, then rest it over the group's half.
    context=$(kad workspaceContext)
    read -r gx gy gw gh < <(jq -r '[.applications[] | select(.hasCard and .stackSize == 2 and .spreadRect)][0].spreadRect
        | "\(.x | floor) \(.y | floor) \(.width | floor) \(.height | floor)"' <<<"$context")
    pick=$(jq -r --arg id "$held" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | (([.x, 0] | max) + ([.x + .width, 1280] | min)) / 2 | floor' <<<"$context")
    y=$((gy + gh / 2))
    if [[ $half == left ]]; then target=$((gx + gw / 4)); else target=$((gx + gw * 3 / 4)); fi
    echo "$half: group at $gx $gy ${gw}x$gh, card picked at $pick $y, aiming its middle at $target"
    probe down 61 "$pick" "$y"
    sleep .4
    finger=$((pick < gx ? pick + 30 : pick - 30))
    probe motion 61 "$finger" "$y"
    sleep .5
    heldX=$(kad nativeCarryState | jq -r '.lineRect | (.x + .width / 2) | floor')
    probe motion 61 "$((finger + target - heldX))" "$y"
    sleep .9
    echo "$half: over the group $(kad nativeCarryState | jq -c '{lineCarrying, carryAim, carryIndex, lineRect}')"
    probe up 61
    sleep 1.2
    report "$half-dropped"
    check "$half: the card joins the group" member "$held" "$stays"
    check "$half: the pane on that half becomes a card of its own" alone "$replaced"
    check "$half: Spread stays open" context '.cardStage.presentation == "cardLine"'

    # Called forward, the layout shows the card on the half it was let go on.
    probe activateWindowId "$stays" >/dev/null
    sleep 1
    report "$half-presented"
    echo "$half: held card at $(frame "$held" | jq -c '{x, width}'), staying pane at $(frame "$stays" | jq -c '{x, width}'), replaced pane at $(frame "$replaced" | jq -c '{x, width}')"
    check "$half: the layout holds two panes" twoPanes
    check "$half: the card is a pane of it, not a card" \
        context --arg id "$held" '.cardStage.presentation == "bento"
            and ([.applications[] | select(.windowId == $id and .hasCard)] | length == 0)'
    if [[ $half == left ]]; then
        check 'left: the card stands wholly left of the other pane' leftOf "$held" "$stays"
    else
        check 'right: the card stands wholly right of the other pane' leftOf "$stays" "$held"
    fi
    check "$half: the other pane keeps its side" \
        test "$(frame "$stays" | jq '.x < 640')" = "$([[ $half == right ]] && echo true || echo false)"

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    probe contactDrop
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: spread bento drop: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card let go on a half of the Bento group in Spread replaces the pane on that half'
