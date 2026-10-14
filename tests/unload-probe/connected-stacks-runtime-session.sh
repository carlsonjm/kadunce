#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11 on one 2560x1440 monitor with no touchscreen, sized
# Connected by its room: four panes, a Stack of three in each. In Spread, as
# on a tablet, cards let go on a pane's card join its Stack, and a pane's own
# card is held and let go on another. The rest are built in the layout: a
# pane carried onto another and rested there joins its Stack, its slot given
# up, while one let go at once swaps. A pane's title bar held, then drawn up or
# the wheel turned, puts it deeper in its Stack. A pane's card pulled down in
# Spread leaves its Stack, and the Stack keeps the pane. Every check is reported, so a failure does not
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
# Every pane holds a Stack of `n` cards behind it.
each_behind() {
    context --argjson n "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))
        | .paneStack | length] | length >= 1 and all(. == $n)'
}
# A window opened while the layout shows joins it; one that stays a card is
# picked into it from Spread.
open_window() {
    client titledCompanion "$1" 500 400
    sleep 1
    w[$2]=$(idOf "$1")
    if ! is_pane "${w[$2]}" >/dev/null 2>&1; then
        echo "state opened-as-card-$1"
        spread_key
        pick "${w[$2]}"
    fi
}
spread_left=0
# Where a person presses a card of Spread: the middle of the part of it in
# view, or nothing while it is out of view.
seen_at() {
    kad workspaceContext | jq -r --arg id "$1" --argjson left "$spread_left" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | select(. != null) | [([.x, $left] | max), ([.x + .width, $left + 2560] | min)] as [$l, $r]
        | select($r - $l > 120) | "\((($l + $r) / 2) | floor) \((.y + .height / 2) | floor)"'
}
entry_of() { kad workspaceContext | jq -r --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .entry'; }
carry_aim() { kad nativeCarryState | jq -r '"\(.carryAim) \(.carryIndex)"'; }
# A person's mouse carries one card onto another in Spread and lets go. Spread
# shows one whole card and its neighbours' edges, so the card is carried as a
# person does: pulled down, the row zooms out. Where all of it shows, the card
# goes straight over the other; where it does not, the card leans just past
# the middle and the row creeps along until the other joins (CarryRow.h).
carry_onto() {
    local trail="" from=$1 to=$2 x y h e entries t items scale width pitch mid x0 dx aim at k
    select_card "$from" >/dev/null 2>&1
    sleep .4
    read -r x y < <(seen_at "$from")
    width=$(kad workspaceContext | jq -r --arg id "$from" 'first(.applications[] | select(.windowId == $id)) | .spreadRect.width')
    h=$(entry_of "$from")
    e=$(entry_of "$to")
    entries=$(kad workspaceContext | jq '[.applications[] | .entry | select(. != null and . >= 0)] | max + 1')
    items=$((entries - 1))
    t=$(( e > h ? e - 1 : e ))
    echo "state carry-plan $from h=$h e=$e t=$t $(kad workspaceContext | jq -c '[.applications[] | select(.entry != null) | "\(.title):\(.entry):\(.stackSize)"]')"
    mid=$((spread_left + 1280))
    probe pointer "$x" "$y"
    probe button true
    sleep .4
    probe pointer "$((x + 30))" "$y"
    sleep .3
    y=$((y + 180))
    probe pointer "$((x + 30))" "$y"
    sleep .8
    scale=$(kad nativeCarryState | jq -r '.carryScale')
    # A pitch is a card and its gutter; the place the card left is a gap 0.7
    # of a pitch wide.
    pitch=$(jq -n --argjson w "$width" '$w * 1.1037')
    if jq -ne --argjson s "$scale" '$s > 0.325' >/dev/null; then
        # All of it shows: the row stands centred.
        x0=$(jq -n --argjson t "$t" --argjson h "$h" --argjson n "$items" --argjson s "$scale" --argjson p "$pitch" --argjson m "$mid" \
            '$m + (($t + (if $t >= $h then 0.7 else 0 end)) - ($n - 1 + 0.7) / 2) * $p * $s | floor')
    else
        # Lean the row just past the middle, as a hand does: it creeps under
        # the card, still parting and joining, until the other card comes
        # under it.
        x0=$(( t >= h ? mid + 290 : mid - 290 ))
        probe pointer "$x0" "$y"
        dx=0
        for _ in {1..400}; do
            sleep .05
            read -r aim at < <(carry_aim)
            [[ ${trail##* } == "$aim$at" ]] || trail+=" $aim$at"
            [[ $aim == join && $at == "$t" ]] && break
        done
    fi
    if jq -ne --argjson s "$scale" '$s > 0.325' >/dev/null; then
        for dx in 0 -40 40 -80 80 -120 120 -160 160; do
            probe pointer "$((x0 + dx))" "$y"
            sleep .5
            read -r aim at < <(carry_aim)
            trail+=" $((x0 + dx)):$aim$at"
            [[ $aim == join && $at == "$t" ]] && break
        done
        sleep .4
    fi
    echo "state carry-trail $trail"
    echo "state carry-$from $((x0 + dx)) $(kad nativeCarryState | jq -c '{lineCarrying, carryAim, carryIndex, carryScale}')"
    probe button false
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
    echo "state drag-$id $(jq -c '{stackOnto, destinationRect}' "$XDG_RUNTIME_DIR/carry")"
    probe contactButton false >/dev/null
    sleep 1.2
}
aimed() { jq -e --arg id "$1" '.stackOnto == $id' "$XDG_RUNTIME_DIR/carry"; }
# A pane's title bar pressed and held, then drawn up `steps` cards' travel and
# the wheel turned `wheel` notches (up for less than nothing); what the scrub
# would do is kept to read afterwards.
scrub_up() {
    local id=$1 title=$2 steps=$3 wheel=${4:-0} x y dy
    read -r x y <<<"$(probe windowGeometry "$id" | jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv')"
    probe activateWindowId "$id" >/dev/null
    sleep .3
    probe pointer "$x" "$y" >/dev/null
    client armMoveOf "$title"
    probe contactButton true >/dev/null
    sleep .6
    for (( dy = 4; dy <= 20 + steps * 56; dy += 12 )); do
        probe contactMotion "$x" "$((y - dy))" >/dev/null
        sleep .03
    done
    if (( wheel != 0 )); then probe wheel "$wheel" >/dev/null; sleep .1; fi
    kad nativeCarryState >"$XDG_RUNTIME_DIR/carry"
    echo "state scrub-$id $(jq -c '{scrubPlace, scrubLadder}' "$XDG_RUNTIME_DIR/carry")"
    probe contactButton false >/dev/null
    sleep 1.2
}
scrubbing() { jq -e --argjson place "$1" --argjson ladder "$2" '.scrubPlace == $place and .scrubLadder == $ladder' "$XDG_RUNTIME_DIR/carry"; }
# The same by KWin's own title bar, as a person presses one: the mouse held
# `hold` seconds, past the time KWin starts a held title bar's move on its own,
# then drawn up `steps` cards' travel and the wheel turned `wheel` notches.
title_of() { probe titleBarOf "$1" | jq -r '[.x, .y] | map(floor) | @tsv'; }
title_scrub() {
    local id=$1 steps=$2 hold=$3 wheel=${4:-0} x y dy
    read -r x y <<<"$(title_of "$id")"
    probe activateWindowId "$id" >/dev/null
    sleep .3
    probe pointer "$x" "$y" >/dev/null
    probe button true >/dev/null
    sleep "$hold"
    if (( steps > 0 )); then
        for (( dy = 4; dy <= 20 + steps * 56; dy += 12 )); do
            probe pointer "$x" "$((y - dy))" >/dev/null
            sleep .03
        done
    fi
    if (( wheel != 0 )); then probe wheel "$wheel" >/dev/null; sleep .1; fi
    kad nativeCarryState >"$XDG_RUNTIME_DIR/carry"
    echo "state title-scrub-$id $(jq -c '{scrubPlace, scrubLadder, lineCarrying}' "$XDG_RUNTIME_DIR/carry")"
    echo "state trace-title-scrub-$id $(kad nativeMoveTrace | jq -Rsc 'split("\n") | map(select(length > 0)) | .[-6:]')"
    probe button false >/dev/null
    sleep 1.2
}
# A pane carried by its title bar onto another pane, held `hold` seconds
# first and let go after `rest` seconds there.
title_drag() {
    local id=$1 to=$2 hold=$3 rest=$4 x y tx ty
    read -r x y <<<"$(title_of "$id")"
    read -r tx ty <<<"$(probe windowGeometry "$to" | jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv')"
    probe activateWindowId "$id" >/dev/null
    sleep .3
    probe pointer "$x" "$y" >/dev/null
    probe button true >/dev/null
    sleep "$hold"
    probe pointer "$((x + 40))" "$((y + 20))" >/dev/null
    sleep .1
    probe pointer "$(((x + tx) / 2))" "$(((y + ty) / 2))" >/dev/null
    sleep .1
    probe pointer "$tx" "$ty" >/dev/null
    sleep "$rest"
    kad nativeCarryState >"$XDG_RUNTIME_DIR/carry"
    echo "state title-drag-$id $(jq -c '{stackOnto, destinationRect, lineCarrying}' "$XDG_RUNTIME_DIR/carry")"
    echo "state trace-title-drag-$id $(kad nativeMoveTrace | jq -Rsc 'split("\n") | map(select(length > 0)) | .[-6:]')"
    probe button false >/dev/null
    sleep 1.2
}
# Every window is a pane or a card, never an ordinary window left over.
all_held() {
    context '[.applications[] | select(.title != "unload-client") | select((.hasCard | not) and (.pane | not) and (.minimized | not))] | length == 0'
}

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
for n in {1..5}; do client titledCompanion "S$n" 500 400; sleep .3; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
declare -A w
for n in {1..5}; do w[$n]=$(idOf "S$n"); done
probe activateWindowId "${w[1]}" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
check 'start: the monitor is sized Connected' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce displaySizing Virtual-0) == connected ]]"

# Three panes, two cards waiting in Spread.
for n in 2 3; do spread_key; pick "${w[$n]}"; done
report three
check 'three: the layout shows' layout_shown
check 'three: three panes' panes 3

# In Spread, as on a tablet: cards let go on a pane's card join its Stack.
spread_key
check 'spread: Spread opens' presents cardLine
carry_onto "${w[4]}" "${w[1]}"
carry_onto "${w[5]}" "${w[1]}"
report spread-stacked
check 'spread: two cards let go on a pane card make a Stack of three' one_stack "[\"${w[1]}\",\"${w[4]}\",\"${w[5]}\"]"
# A pane's own card is held as any card, and let go on another pane's card
# joins its Stack.
carry_onto "${w[3]}" "${w[2]}"
report spread-pane-held
check 'spread: a pane card held and let go on another joins its Stack' one_stack "[\"${w[3]}\",\"${w[2]}\"]"
select_card "${w[3]}"
open_selected
report spread-open
check 'spread open: the layout shows' layout_shown
check 'spread open: two panes, the held pane gave its slot up' panes 2
check 'spread open: the held card fills the pane it joined' is_pane "${w[3]}"
check 'spread open: the card it covered waits behind it' behind_is "${w[3]}" "[\"${w[2]}\"]"
check 'spread open: the Stack of three fills one pane' behind_count "${w[5]}" 2

# In the layout: a pane carried onto another and rested joins its Stack.
open_window S6 6
report sixth
check 'sixth: a window opened joins the layout' panes 3
drag_onto "${w[6]}" S6 "${w[3]}" .9
report rested
check 'rest: the carry names the pane it would join' aimed "${w[3]}"
check 'rest: the carried pane is a pane' is_pane "${w[6]}"
check 'rest: the pane it joined waits behind it as a card' is_card "${w[3]}"
check 'rest: the Stack waits behind it, front to back' behind_is "${w[6]}" "[\"${w[3]}\",\"${w[2]}\"]"
check 'rest: the slot it left is given up' panes 2
open_window S7 7
open_window S10 10
report swap-ready
check 'two opened: four panes' panes 4
# Let go at once, two panes swap as before.
slot7=$(rect "${w[7]}") slot10=$(rect "${w[10]}")
drag_onto "${w[10]}" S10 "${w[7]}" .1
report swapped
check 'quick: the carry names no Stack' jq -e '.stackOnto == ""' "$XDG_RUNTIME_DIR/carry"
check 'quick: the carried pane takes the other place' same_rect "${w[10]}" "$slot7"
check 'quick: and the other takes its place' same_rect "${w[7]}" "$slot10"
check 'quick: still four panes' panes 4
for n in 8 9 11 12; do
    open_window "S$n" "$n"
    drag_onto "${w[$n]}" "S$n" "${w[$((n - 1))]}" .9
done
report built
check 'built: four panes' panes 4
check 'built: the last Stack waits behind its pane, front to back' behind_is "${w[12]}" "[\"${w[11]}\",\"${w[10]}\"]"
check 'built: every pane holds a Stack of three' each_behind 2

# A pane's title bar held, then drawn up, passes the cards behind it; let go,
# it goes in there and the card it passed first fills the pane.
scrub_up "${w[6]}" S6 2
report scrubbed
check 'scrub: held and drawn up two cards, it would go in last' \
    scrubbing 2 "[\"${w[3]}\",\"${w[2]}\",\"${w[6]}\"]"
check 'scrub: the card it passed first fills the pane' is_pane "${w[3]}"
check 'scrub: it went in at the back' behind_is "${w[3]}" "[\"${w[2]}\",\"${w[6]}\"]"
check 'scrub: still four panes' panes 4
# The wheel, turned away while held, passes one card a notch.
scrub_up "${w[3]}" S3 0 -1
report wheeled
check 'wheel: one notch up, it would go in second' scrubbing 1 "[\"${w[2]}\",\"${w[3]}\",\"${w[6]}\"]"
check 'wheel: the card it passed fills the pane' is_pane "${w[2]}"
check 'wheel: it went in second' behind_is "${w[2]}" "[\"${w[3]}\",\"${w[6]}\"]"

# By KWin's own title bars, as real applications have them: held past the time
# KWin starts the move itself, the pane still reorders, and a held pane carried
# sideways still swaps or stacks as a pane rather than becoming a window.
for n in {1..12}; do probe serverDecorate "${w[$n]}" >/dev/null; done
sleep 1.5
report decorated
check 'title: every pane has a title bar of KWin'"'"'s' bash -c "for id in ${w[2]} ${w[9]} ${w[12]}; do qdbus6 org.kde.KWin /UnloadProbe titleBarOf \$id | jq -e '.height > 0' || exit 1; done"
check 'title: still four panes' panes 4
title_scrub "${w[2]}" 1 .9
report title-scrubbed
check 'title held: drawn up one card, it would go in second' scrubbing 1 "[\"${w[3]}\",\"${w[2]}\",\"${w[6]}\"]"
check 'title held: the card it passed fills the pane' is_pane "${w[3]}"
check 'title held: it went in second' behind_is "${w[3]}" "[\"${w[2]}\",\"${w[6]}\"]"
title_scrub "${w[3]}" 0 .9 -2
report title-wheeled
check 'title wheel: held still, two notches up, it would go in last' scrubbing 2 "[\"${w[2]}\",\"${w[6]}\",\"${w[3]}\"]"
check 'title wheel: it went in last' behind_is "${w[2]}" "[\"${w[6]}\",\"${w[3]}\"]"
check 'title wheel: still four panes' panes 4
slot2=$(rect "${w[2]}") slot9=$(rect "${w[9]}")
title_drag "${w[2]}" "${w[9]}" .9 .1
report title-swapped
check 'title swap: held, then carried and let go at once, it swaps' same_rect "${w[2]}" "$slot9"
check 'title swap: and the other takes its place' same_rect "${w[9]}" "$slot2"
check 'title swap: still four panes' panes 4
check 'title swap: every Stack stays whole' each_behind 2
check 'title swap: no window is left ordinary' all_held
title_drag "${w[9]}" "${w[12]}" .1 .9
report title-stacked
check 'title stack: rested on a pane, it joins its Stack in front' behind_is "${w[9]}" "[\"${w[12]}\",\"${w[11]}\",\"${w[10]}\"]"
check 'title stack: its own next card keeps its slot' is_pane "${w[8]}"
check 'title stack: still four panes' panes 4
check 'title stack: no window is left ordinary' all_held

# A pane's card pulled down in Spread leaves its Stack; the Stack keeps the
# pane with its next card.
spread_key
select_card "${w[12]}"
sleep 1
read -r x y < <(kad workspaceContext | jq -r --arg id "${w[12]}" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
    | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
probe down 62 "$x" "$y"
sleep .1
for step in 40 100 180 260 340; do probe motion 62 "$x" "$((y + step))"; sleep .08; done
sleep .2
probe up 62
sleep 1.2
report pulled
check 'pull: the card left its Stack' context --arg id "${w[12]}" 'first(.applications[] | select(.windowId == $id)) | .stackSize == 1'
check 'pull: the Stack stays whole without it' one_stack "[\"${w[11]}\",\"${w[10]}\"]"
select_card "${w[11]}"
open_selected
report pulled-open
check 'pull: the Stack keeps the pane' is_pane "${w[11]}"
check 'pull: still four panes' panes 4
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
