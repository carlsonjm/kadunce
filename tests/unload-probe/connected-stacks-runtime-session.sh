#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11 on one 2560x1440 monitor with no touchscreen, sized
# Connected by its room: four panes, a Stack of three in each. Two are built
# in Spread, a card let go on a pane's card, and one by holding a pane's own
# card, as on a tablet. The rest are built in the layout: a pane carried onto
# another and rested there joins its Stack, its slot given up, while one let
# go at once swaps. A pane's card pulled down in Spread leaves its Stack, and
# the Stack keeps the pane. Every check is reported, so a failure does not
# hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected stacks: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, hasCard, pane, paneStack, stackId, stackSize, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, x, y, width, height, hidden})')"
}
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
spread_key() { probe key 31 125 >/dev/null; sleep 1; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.2; }
selected() { [[ $(kad workspaceContext | jq -r '.cardStage.selectedCardId') == "$1" ]]; }
select_card() {
    local id=$1 key
    for key in 105 106; do
        for _ in {1..14}; do
            selected "$id" && return 0
            probe key "$key" 0 >/dev/null
            sleep .3
        done
    done
    selected "$id"
}
pick() { select_card "$1" || echo "could not select $1"; open_selected; }
is_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | (.hasCard | not) and (.minimized | not)'; }
is_card() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard'; }
panes() { context --argjson n "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))] | length == $n'; }
layout_shown() { context '.cardStage.presentation == "bento" and ([.displayContext.displays[] | select(.bentoActive)] | length == 1)'; }
# The cards waiting behind a pane, front to back, as one list of ids.
behind_is() { context --arg id "$1" --argjson ids "$2" 'first(.applications[] | select(.windowId == $id)) | .paneStack == $ids'; }
behind_count() { context --arg id "$1" --argjson n "$2" 'first(.applications[] | select(.windowId == $id)) | (.paneStack | length) == $n'; }
one_stack() { context --argjson ids "$1" '[.applications[] | select(.windowId as $w | $ids | index($w)) | .stackId] | (length == ($ids | length) and (unique | length) == 1)'; }
rect() { probe windowFacts | jq -c --arg id "$1" 'first(.[] | select(.id == $id)) | {x, y, width, height}'; }
same_rect() { [[ $(rect "$1") == "$2" ]]; }
# Four panes in two columns, each half the monitor wide.
quadrants() {
    context '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not)) | .windowId] | length == 4' \
        && probe windowFacts | jq -e '[.[] | select(.class == "unload-client" and (.hidden | not) and .caption != "unload-client")
            | select(.width > 1100 and .width < 1300 and .height > 300)] | length == 4'
}
# A person's finger carries one card onto another in Spread and lets go.
carry_onto() {
    local from=$1 to=$2 x y tx ty
    read -r x y < <(kad workspaceContext | jq -r --arg id "$from" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
    probe down 61 "$x" "$y"
    sleep .4
    probe motion 61 "$((x + 30))" "$y"
    sleep .5
    read -r tx ty < <(kad workspaceContext | jq -r --arg id "$to" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
    probe motion 61 "$tx" "$ty"
    sleep .7
    echo "carry $(kad nativeCarryState | jq -c '{lineCarrying, carryAim}')"
    probe up 61
    sleep .8
}
# A pane carried by a press inside it onto another pane, let go after `rest`
# seconds there; what the carry would do is kept to read afterwards.
drag_onto() {
    local id=$1 title=$2 to=$3 rest=$4 x y tx ty
    read -r x y <<<"$(probe windowGeometry "$id" | jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv')"
    read -r tx ty <<<"$(probe windowGeometry "$to" | jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv')"
    probe activateWindowId "$id" >/dev/null
    sleep .3
    probe pointer "$x" "$y" >/dev/null
    client armMoveOf "$title"
    probe contactButton true >/dev/null
    sleep .12
    probe contactMotion "$((x + 40))" "$((y + 20))" >/dev/null
    sleep .1
    probe contactMotion "$tx" "$ty" >/dev/null
    sleep "$rest"
    kad nativeCarryState >"$XDG_RUNTIME_DIR/carry"
    echo "carry $(jq -c '{stackOnto, destinationRect}' "$XDG_RUNTIME_DIR/carry")"
    probe contactButton false >/dev/null
    sleep 1.2
}
aimed() { jq -e --arg id "$1" '.stackOnto == $id' "$XDG_RUNTIME_DIR/carry"; }

for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 1280 720
probe contactObserve >/dev/null
probe contactStart >/dev/null
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for n in {1..12}; do client titledCompanion "S$n" 500 400; sleep .3; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
declare -A w
for n in {1..12}; do w[$n]=$(idOf "S$n"); done
probe activateWindowId "${w[1]}" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
check 'start: the monitor is sized Connected' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce displaySizing Virtual-0) == connected ]]"

# Four panes: three picks grow the layout to quadrants.
for n in 2 3 4; do spread_key; pick "${w[$n]}"; done
report four
check 'four: the layout shows' layout_shown
check 'four: four panes in two columns' quadrants

# In Spread, as on a tablet: cards let go on a pane's card join its Stack.
spread_key
check 'spread: Spread opens' presents cardLine
for pair in '5 1' '6 1' '7 2' '8 2'; do
    read -r from to <<<"$pair"
    carry_onto "${w[$from]}" "${w[$to]}"
done
report spread-stacked
check 'spread: the first pane card holds a Stack of three' one_stack "[\"${w[1]}\",\"${w[5]}\",\"${w[6]}\"]"
check 'spread: the second pane card holds a Stack of three' one_stack "[\"${w[2]}\",\"${w[7]}\",\"${w[8]}\"]"
# A pane's own card is held as any card, and let go on another pane's card
# joins its Stack.
carry_onto "${w[3]}" "${w[4]}"
report spread-pane-held
check 'spread: a pane card held and let go on another joins its Stack' one_stack "[\"${w[3]}\",\"${w[4]}\"]"
select_card "${w[3]}"
open_selected
report spread-open
check 'spread open: the layout shows' layout_shown
check 'spread open: three panes, the held pane gave its slot up' panes 3
check 'spread open: the held card fills the pane it joined' is_pane "${w[3]}"
check 'spread open: its Stack waits behind it' behind_is "${w[3]}" "[\"${w[4]}\"]"
check 'spread open: the first Stack fills one pane' behind_count "${w[6]}" 2
check 'spread open: the second Stack fills one pane' behind_count "${w[8]}" 2

# In the layout: a pane carried onto another and rested joins its Stack.
spread_key
pick "${w[9]}"
report ninth
check 'ninth: the pick joins as the fourth pane' panes 4
drag_onto "${w[9]}" S9 "${w[3]}" .9
report rested
check 'rest: the carry names the pane it would join' aimed "${w[3]}"
check 'rest: the carried pane is a pane' is_pane "${w[9]}"
check 'rest: the pane it joined waits behind it as a card' is_card "${w[3]}"
check 'rest: the Stack waits behind it, front to back' behind_is "${w[9]}" "[\"${w[3]}\",\"${w[4]}\"]"
check 'rest: the slot it left is given up' panes 3
# Let go at once, two panes swap as before.
spread_key
pick "${w[10]}"
slot10=$(rect "${w[10]}") slot9=$(rect "${w[9]}")
drag_onto "${w[10]}" S10 "${w[9]}" .1
report swapped
check 'quick: the carry names no Stack' jq -e '.stackOnto == ""' "$XDG_RUNTIME_DIR/carry"
check 'quick: the carried pane takes the other place' same_rect "${w[10]}" "$slot9"
check 'quick: and the other takes its place' same_rect "${w[9]}" "$slot10"
check 'quick: still four panes' panes 4
for n in 11 12; do
    spread_key
    pick "${w[$n]}"
    drag_onto "${w[$n]}" "S$n" "${w[$((n - 1))]}" .9
done
report built
check 'built: four panes in two columns' quadrants
check 'built: the last Stack waits behind its pane' behind_is "${w[12]}" "[\"${w[11]}\",\"${w[10]}\"]"
check 'built: every pane holds a Stack of three' \
    context '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not)) | .paneStack | length] == [2,2,2,2]'

# A pane's card pulled down in Spread leaves its Stack; the Stack keeps the
# pane with its next card.
spread_key
select_card "${w[12]}"
read -r x y < <(kad workspaceContext | jq -r --arg id "${w[12]}" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
    | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
probe down 62 "$x" "$y"
for step in 60 140 220 300; do probe motion 62 "$x" "$((y + step))"; sleep .05; done
probe up 62
sleep 1
report pulled
check 'pull: the card left its Stack' context --arg id "${w[12]}" 'first(.applications[] | select(.windowId == $id)) | .stackSize == 1'
check 'pull: the Stack stays whole without it' one_stack "[\"${w[11]}\",\"${w[10]}\"]"
select_card "${w[11]}"
open_selected
report pulled-open
check 'pull: the Stack keeps the pane' panes 4
check 'pull: the pulled card is no pane' is_card "${w[12]}"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
check 'switched off: every window is an ordinary window again' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 12 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
if (( failures > 0 )); then
    echo "FAIL: connected stacks: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: Connected builds four panes holding a Stack of three each, in Spread and in the layout'
