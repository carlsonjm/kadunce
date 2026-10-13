#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11 on one 2560x1440 monitor with no touchscreen, sized
# Connected by its room. Spread opens zoomed out, every card whole on screen,
# and every window is a card of its own, a layout's panes included. A picked
# card joins the layout beside the card in use and the layout grows; full,
# the pane used longest ago leaves and becomes a card; a picked pane opens the
# layout on it. A Stack fills one pane, and Meta+Down steps it in place. The
# per-display choice is kept in kaduncerc and Active sizing brings back the
# Bento group. Every check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        displays: [.displayContext.displays[] | {name, role, sizing, sizingChosen, bentoActive}],
        apps: [.applications[] | {title, hasCard, stackId, stackSize, focused, minimized, spreadRect}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, x, y, width, height, hidden})')"
}
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
# Meta+S by evdev code; Spread's own keys move along the row and open a card.
spread_key() { probe key 31 125 >/dev/null; sleep 1; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.2; }
selected() { [[ $(kad workspaceContext | jq -r '.cardStage.selectedCardId') == "$1" ]]; }
# The row has two ends, so the keys walk to one end and then to the other.
select_card() {
    local id=$1 key
    for key in 105 106; do
        for _ in {1..8}; do
            selected "$id" && return 0
            probe key "$key" 0 >/dev/null
            sleep .35
        done
    done
    selected "$id"
}
pick() { select_card "$1" || echo "could not select $1"; open_selected; }
is_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | (.hasCard | not) and (.minimized | not)'; }
both_panes() { is_pane "$1" && is_pane "$2"; }
is_card() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard'; }
panes() { context --argjson n "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))] | length == $n'; }
layout_shown() { context '.cardStage.presentation == "bento" and ([.displayContext.displays[] | select(.bentoActive)] | length == 1)'; }
focused() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .focused'; }
# Every card stands alone: no window of the layout hides inside a group card.
each_its_own() {
    context '[.applications[] | select(.hasCard and (.minimized | not))] as $c
        | ($c | length) >= 2 and all($c[]; .stackSize == 1) and ([$c[].stackId] | unique | length) == ($c | length)'
}
# Zoomed out, every card of the row shows whole on the display.
all_whole() {
    context '[.applications[] | select(.hasCard and (.minimized | not))] as $c
        | ($c | length) >= 2 and all($c[]; .spreadRect and .spreadRect.x >= 0 and .spreadRect.x + .spreadRect.width <= 2560)'
}
rect() { probe windowFacts | jq -c --arg id "$1" 'first(.[] | select(.id == $id)) | {x, y, width, height}'; }
same_rect() { [[ $(rect "$1") == "$2" ]]; }
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
start_pass() {
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    sleep 1
    local width=$1 height=$2; shift 2
    for title in "$@"; do client titledCompanion "$title" "$width" "$height"; sleep .5; done
    sleep .5
    # The client's own window sleeps, so only the named windows take part.
    probe minimizeWindow "$(idOf unload-client)" true >/dev/null
    sleep .4
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1.2
}
end_pass() {
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .8
    check "$1: switched off, every window is an ordinary window again" \
        bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length >= 2 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
    kill "$client_pid" 2>/dev/null || true
    wait "$client_pid" 2>/dev/null || true
    sleep .5
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 1280 720

# Pass one: windows small enough that the layout grows with every pick.
start_pass 500 400 'Card A' 'Card B' 'Card C' 'Card D'
a=$(idOf 'Card A') b=$(idOf 'Card B') c=$(idOf 'Card C') d=$(idOf 'Card D')
probe activateWindowId "$d" >/dev/null
sleep .8
report start
check 'by room: the monitor is sized Connected' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce displaySizing Virtual-0) == connected ]]"
check 'by room: the context names it, and nothing was chosen' \
    context '[.displayContext.displays[] | select(.name == "Virtual-0" and .sizing == "connected" and (.sizingChosen | not))] | length == 1'
check 'start: the card in use is Active' context --arg id "$d" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
spread_key
report spread
check 'spread: Spread opens' presents cardLine
check 'spread: zoomed out, every card shows whole' all_whole
pick "$a"
report paired
check 'pick: the layout shows' layout_shown
check 'pick: the picked card is a pane' is_pane "$a"
check 'pick: the card in use is the other pane' is_pane "$d"
check 'pick: two panes' panes 2
check 'pick: the picked pane has focus' focused "$a"
spread_key
report spread-layout
check 'spread over a layout: Spread opens' presents cardLine
check 'spread over a layout: every pane is a card of its own, no group card' each_its_own
check 'spread over a layout: every card shows whole' all_whole
pick "$b"
report grown
check 'grow: the layout shows' layout_shown
check 'grow: three panes' panes 3
check 'grow: the picked card is a pane' is_pane "$b"
check 'grow: the earlier panes stay panes' both_panes "$a" "$d"
before=$(rect "$d")
spread_key
pick "$d"
report pane-picked
check 'pane picked: the layout shows' layout_shown
check 'pane picked: it has focus' focused "$d"
check 'pane picked: nothing left the layout' panes 3
check 'pane picked: its pane stands where it stood' same_rect "$d" "$before"

# The choice is the person's, per display, and written down.
kad setDisplaySizing Virtual-0 active
sleep .6
report chosen-active
check 'chosen: the monitor is sized Active' context '[.displayContext.displays[] | select(.name == "Virtual-0" and .sizing == "active" and .sizingChosen)] | length == 1'
check 'chosen: kaduncerc keeps it' bash -c "kreadconfig6 --file kaduncerc --group Sizing --key Virtual-0 | grep -qx active"
spread_key
report active-spread
check 'active sizing: the layout is one group card in Spread again' \
    context '[.applications[] | select(.hasCard and .stackSize >= 3)] | length >= 3'
spread_key
kad setDisplaySizing Virtual-0 ''
sleep .6
check 'automatic again: sized by room' context '[.displayContext.displays[] | select(.name == "Virtual-0" and .sizing == "connected" and (.sizingChosen | not))] | length == 1'
check 'automatic again: kaduncerc no longer names it' bash -c "[[ -z \$(kreadconfig6 --file kaduncerc --group Sizing --key Virtual-0) ]]"
end_pass grow

# Pass two: windows so large that two panes fill the monitor.
start_pass 1100 900 'Card A' 'Card B' 'Card C'
a=$(idOf 'Card A') b=$(idOf 'Card B') c=$(idOf 'Card C')
probe activateWindowId "$b" >/dev/null
sleep .8
spread_key
pick "$a"
report full-pair
check 'full: two panes' panes 2
check 'full: the card in use and the pick are the panes' both_panes "$a" "$b"
slot_b=$(rect "$b")
spread_key
pick "$c"
report full-yield
check 'full: the layout shows' layout_shown
check 'full: still two panes' panes 2
check 'full: the pick is a pane' is_pane "$c"
check 'full: the pane used longest ago left and is a card' is_card "$b"
check 'full: the pane used last stayed' is_pane "$a"
check 'full: the pick took the slot that was given up' same_rect "$c" "$slot_b"

# A Stack fills one pane. The card that left is let go on a pane's card.
slot_c=$(rect "$c")
spread_key
select_card "$c"
carry_onto "$b" "$c"
report stacked
check 'stack: the card and the pane card are one Stack' \
    context --arg b "$b" --arg c "$c" '[.applications[] | select(.windowId == $b or .windowId == $c) | .stackId] | (length == 2 and (unique | length) == 1)'
open_selected
report stack-open
check 'stack: the layout shows' layout_shown
check 'stack: still two panes' panes 2
check 'stack: the other pane stayed' is_pane "$a"
face=$(kad workspaceContext | jq -r --arg b "$b" --arg c "$c" 'first(.applications[] | select((.windowId == $b or .windowId == $c) and (.hasCard | not))) | .windowId')
other=$([[ $face == "$b" ]] && echo "$c" || echo "$b")
check 'stack: one of the Stack is the pane, in that pane' same_rect "$face" "$slot_c"
check 'stack: the rest of it waits as a card' is_card "$other"
slot_a=$(rect "$a")
probe activateWindowId "$face" >/dev/null
sleep .5
probe key 108 125 >/dev/null
sleep 1.2
report stepped
check 'step: Meta+Down puts the next card of the Stack in the pane' is_pane "$other"
check 'step: in the same place' same_rect "$other" "$slot_c"
check 'step: the card it replaced waits in the Stack' is_card "$face"
check 'step: the other pane did not move' same_rect "$a" "$slot_a"
check 'step: still two panes' panes 2
probe key 103 125 >/dev/null
sleep 1.2
report stepped-back
check 'step back: Meta+Up returns the first card to the pane' is_pane "$face"
check 'step back: in the same place' same_rect "$face" "$slot_c"
face_title=$(probe windowFacts | jq -r --arg id "$face" 'first(.[] | select(.id == $id)) | .caption')
client closeCompanion "$face_title"
sleep 1.5
report closed-front
check 'close: the next card of the Stack takes the pane' is_pane "$other"
check 'close: in the same place' same_rect "$other" "$slot_c"
check 'close: still two panes' panes 2
end_pass full
if (( failures > 0 )); then
    echo "FAIL: connected: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: Connected plays picked cards into the layout, every pane a card of its own in Spread, a Stack fills one pane'
