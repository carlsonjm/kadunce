#!/usr/bin/env bash
# INPUT.md § Bento, by the keys alone, on two monitors with no touchscreen, as
# a desk is used: Virtual-0 is small and left of Virtual-1, which is large,
# sized Connected and chosen for Spread. With a layout of two panes there,
# Table opens and closes, then Meta+B ends the layout into cards, the one in
# use Active; Meta+B again lays out every card open there; and Meta+S
# opens Spread over the layout and closes it again. A pane then carried by
# KWin's title bar to the top edge, out of the three, is the Active card
# shown alone, the other two waiting in Spread, and one Meta+B lays all three
# out again. A pane let
# go to wait as a card, called up in front of the layout Spread holds, joins
# it again by one Meta+B. Every check is reported, so a failure does not hide
# the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected bento key: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, output, hasCard, pane, paneStack, stackId, focused, minimized}]}')"
    echo "stages $1 $(kad outputStageState | tr '\n' ' ')"
}
spread_key() { probe key 31 125 >/dev/null; sleep 1.2; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.4; }
selected() { [[ $(kad workspaceContext | jq -r '.cardStage.selectedCardId') == "$1" ]]; }
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
title_at() { probe titleBarOf "$1" | jq -r 'select(.x != null) | [.x, .y] | map(floor) | @tsv'; }
# A person's mouse presses a window's title bar, KWin's own, and carries it
# through each point given in turn before letting go.
title_drag() {
    local id=$1 x y; shift
    read -r x y <<<"$(title_at "$id")"
    [[ -n ${y:-} ]] || { echo "no title bar on $id"; return 0; }
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe contactButton true >/dev/null
    sleep .1
    probe contactMotion "$((x + 30))" "$((y + 40))" >/dev/null
    sleep .15
    while (( $# >= 2 )); do
        probe contactMotion "$1" "$2" >/dev/null
        shift 2
        sleep .12
    done
    sleep .4
    probe contactButton false >/dev/null
    sleep 1.6
}
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
large_panes() { kad workspaceContext | jq '[.applications[] | select(.pane and (.hasCard | not) and .output == "Virtual-1")] | length'; }
panes_are() { [[ $(large_panes) == "$1" ]]; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
kscreen-doctor output.Virtual-0.scale.2 >/dev/null 2>&1
sleep 1
kwriteconfig6 --file kaduncerc --group Spread --key Display Virtual-1
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for title in 'Small' 'Mail' 'Wallet' 'Browser'; do client titledCompanion "$title" 500 400; sleep .5; done
probe sendCaptionToOutput 'Small' Virtual-0
for title in 'Mail' 'Wallet' 'Browser'; do probe sendCaptionToOutput "$title" Virtual-1; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
mail=$(idOf 'Mail') wallet=$(idOf 'Wallet') browser=$(idOf 'Browser')
for id in "$mail" "$wallet" "$browser"; do probe serverDecorate "$id" >/dev/null; done
sleep .4
probe activateWindowId "$mail" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
spread_key
pick "$wallet"
report start
check 'start: the large display shows a layout of two panes' panes_are 2

# Meta+S over the layout opens Spread, and Escape goes back to it.
spread_key
report spread
check 'spread: Meta+S opens Spread over the layout' presents cardLine
probe key 1 0 >/dev/null
sleep 1.2
report spread-back
check 'spread: Escape goes back to the layout' panes_are 2

# Table opens and closes by its key; then Meta+B ends the layout.
probe key 17 125 >/dev/null
sleep 1.2
report table
probe key 17 125 >/dev/null
sleep 1.2
report table-closed
check 'table: Meta+W twice leaves the layout as it was' panes_are 2
probe key 48 125 >/dev/null
sleep 1.6
report ended
check 'end: Meta+B ends the layout' panes_are 0
check 'end: its windows are cards, one of them Active' context '.cardStage.presentation == "active" and .cardStage.selectedCardId != null'
probe key 48 125 >/dev/null
sleep 1.6
report paired
check 'fill: Meta+B again lays out every open card of the display' panes_are 3
spread_key
report spread-again
check 'spread: Meta+S still opens Spread' presents cardLine
check 'spread: the panes are cards of their own there, no two in one Stack' context '[.applications[] | select(.output == "Virtual-1" and .stackId != null) | .stackId] | length == (unique | length)'
probe key 1 0 >/dev/null
sleep 1.2
report final
check 'spread: and Escape goes back to the layout' panes_are 3

# Carried by its title bar to the top edge, a pane taken out of three is the
# Active card in front of the two left. One Meta+B lays all three out again,
# the carried one among them, rather than ending the two it stood in front of.
read -r tx _ <<<"$(title_at "$browser")"
title_drag "$browser" "$((tx + 30))" 200 "${tx:-0}" 100 "${tx:-0}" 40 "${tx:-0}" 12 "${tx:-0}" 2
report top
check 'top: the pane carried there is the Active card' \
    context --arg id "$browser" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
check 'top: the two it left wait in Spread, none live behind it' panes_are 0
probe key 48 125 >/dev/null
sleep 1.6
report back-in
check 'back: one Meta+B lays out every open card again' panes_are 3
check 'back: the card carried to the top is a pane of it' \
    context --arg id "$browser" 'first(.applications[] | select(.windowId == $id)) | .pane and (.hasCard | not)'

# A pane let go with Meta+Shift+Down waits as a card. With Spread open over
# the layout, the card called up by its application, as the dock does, is the
# Active card in front of the layout Spread holds, whose panes are no
# partners for it. One Meta+B lays the layout out again with it.
probe activateWindowId "$browser" >/dev/null
sleep .6
probe keyHeld 108 1 125 42 >/dev/null
sleep 1.4
report waiting
check 'waiting: the pane let go waits as a card' panes_are 2
spread_key
report spread-held
check 'held: Meta+S opens Spread over the layout of two' presents cardLine
probe activateWindowId "$browser" >/dev/null
sleep 1.4
report called
check 'called: the waiting card is Active in front of the layout' \
    context --arg id "$browser" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
probe key 48 125 >/dev/null
sleep 1.6
report held-back-in
check 'held: one Meta+B lays the layout held behind it out again' panes_are 3
check 'held: the Active card is a pane of it' \
    context --arg id "$browser" 'first(.applications[] | select(.windowId == $id)) | .pane and (.hasCard | not)'

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 4 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: connected bento key: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: Meta+B ends and remakes a layout on Connected, lays one standing behind the Active card out again with it, live or held in Spread, and Meta+S opens Spread over it, Table or no Table'
