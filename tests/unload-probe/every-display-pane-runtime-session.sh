#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, as a tablet with a monitor is used:
# Virtual-0 at scale 2 is small and sized Solo, Virtual-1 is large, sized
# Connected, primary, and has a status bar along its top edge. A
# card waiting for the small display, asked for from the dock, opens there,
# its own, from its own Spread. The large display's own card
# and a window opened beside it make a layout, and choosing either pane in
# Spread, by a pick or by Escape, opens that layout whole rather than sending
# the pane away and leaving half a layout. Every move below is made on KWin's
# own title bars. A pane dragged to the top edge, over the bar, is the Active
# card alone; a pane dragged to the small display is the card shown there, and
# the one pane it leaves is the Active card, never half a layout. Meta+B then
# pairs that card with the large display's own card at one press, never with
# one waiting for the small display. Every check is reported, so a failure
# does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: every display pane: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        displays: [.displayContext.displays[] | {name, role, sizing, holdsCards, bentoActive, geometry}],
        apps: [.applications[] | {title, windowId, output, hasCard, pane, stage, focused, minimized}],
        stages: [.displayContext.displays[] | {name, shows, s: .cardStage.presentation, a: .cardStage.activeCardId}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client" and .normal) | {caption, output, x, y, width, height, hidden})')"
    echo "stages $1 $(kad outputStageState | tr '\n' ' ')"
}
spread_key() { probe key 31 125 >/dev/null; sleep 1.2; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.4; }
escape_key() { probe key 1 0 >/dev/null; sleep 1.4; }
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
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
# The large display shows this card, Active, and no layout.
active_card() { context --arg id "$1" 'first(.displayContext.displays[] | select(.name == "Virtual-1")) | .cardStage | .presentation == "active" and .activeCardId == $id'; }
# The card the small display shows on its own card stage, there.
shown_small() { context --arg id "$1" '(first(.displayContext.displays[] | select(.name == "Virtual-0")) | .cardStage | .activeCardId == $id) and (first(.applications[] | select(.windowId == $id)) | .stage == "Virtual-0" and .output == "Virtual-0")'; }
# What the small display shows: its own card, or a pane it should not have.
small_panes() { context --argjson n "$1" 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | (.shows | length) == $n'; }
small_shows() { kad workspaceContext | jq -r 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .cardStage | .activeCardId'; }
# A card of the small display waiting in its own Spread, not shown.
waits_for_small() { context --arg id "$1" '(first(.displayContext.displays[] | select(.name == "Virtual-0")) | .cardStage | .activeCardId != $id) and (first(.applications[] | select(.windowId == $id)) | .hasCard and .stage == "Virtual-0")'; }
# A card or pane of the large display, none of the small display's.
waits_for_none() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .stage != "Virtual-0" and .output == "Virtual-1"'; }
monitor_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .pane and .output == "Virtual-1" and (.hasCard | not)'; }
monitor_panes() { context --argjson n "$1" '[.applications[] | select(.pane and .output == "Virtual-1")] | length == $n'; }
monitor_card() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard and (.pane | not) and .stage == "Virtual-1"'; }
# The large display shows no layout, not even a pane of one.
no_monitor_layout() { context '([.applications[] | select(.pane and .output == "Virtual-1")] | length == 0)
    and (first(.displayContext.displays[] | select(.name == "Virtual-1")) | .bentoActive | not)'; }
# Its two panes fill the large display between them, edge to edge, so neither
# stands alone in half of it.
spans_monitor() {
    local ids g
    ids=$(kad workspaceContext | jq -c '[.applications[] | select(.pane and .output == "Virtual-1") | .windowId]')
    g=$(kad workspaceContext | jq -c 'first(.displayContext.displays[] | select(.name == "Virtual-1")) | .geometry')
    probe windowFacts | jq -e --argjson ids "$ids" --argjson g "$g" 'map(select(.id as $i | any($ids[]; . == $i)))
        | length == 2 and ((map(.x + .width) | max) - (map(.x) | min)) >= $g.width * 0.9'
}
all_held() {
    context '[.applications[] | select(.title != "unload-client") | select((.hasCard | not) and (.pane | not) and (.minimized | not))] | length == 0'
}
title_at() { probe titleBarOf "$1" | jq -r 'select(.x != null) | [.x, .y] | map(floor) | @tsv'; }
has_title() { probe titleBarOf "$1" | jq -e '.height > 0'; }
# A person's click on a window's title bar, KWin's own.
click_title() {
    local x y
    read -r x y <<<"$(title_at "$1")"
    [[ -n ${y:-} ]] || { echo "no title bar on $1"; return 0; }
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe button true >/dev/null
    sleep .05
    probe button false >/dev/null
    sleep 1.2
}
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
    echo "state carry-$id $(kad nativeCarryState | jq -c '{carrying, lineCarrying, destinationRect}')"
    echo "state trace-$id $(kad nativeMoveTrace | jq -Rsc 'split("\n") | map(select(length > 0)) | .[-6:]')"
    probe contactButton false >/dev/null
    sleep 1.6
}
# A window opened while the pointer is on the large display, given KWin's
# title bar as soon as it appears.
open_on_monitor() {
    local title=$1 id=
    probe pointer 3840 720 >/dev/null
    client titledCompanion "$title" 500 400
    for _ in {1..20}; do id=$(idOf "$title"); [[ -n $id ]] && break; sleep .1; done
    probe serverDecorate "$id" >/dev/null
    sleep 1.8
}
for _ in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
kscreen-doctor output.Virtual-0.scale.2 >/dev/null 2>&1
sleep 1
echo "displays: $(kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Output|Geometry|Scale' | tr -s ' \n' ' ')"
kwriteconfig6 --file kaduncerc --group Spread --key Display Virtual-1
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
# A status bar along the large display's top edge, as a panel there is: it
# covers the band a window is carried to for the top edge.
client topBarSurface Virtual-1 24
sleep .5
for title in 'Tab A' 'Tab B' 'Mon A'; do client titledCompanion "$title" 500 400; sleep .5; done
probe sendCaptionToOutput 'Tab A' Virtual-0
probe sendCaptionToOutput 'Tab B' Virtual-0
probe sendCaptionToOutput 'Mon A' Virtual-1
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
ta=$(idOf 'Tab A') tb=$(idOf 'Tab B') ma=$(idOf 'Mon A')
for id in "$ta" "$tb" "$ma"; do probe serverDecorate "$id" >/dev/null; done
sleep .4
probe activateWindowId "$ma" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
report start
check 'start: the large display is primary, sized Connected' \
    context '[.displayContext.displays[] | select(.name == "Virtual-1" and .primary and .sizing == "connected")] | length == 1'
check 'start: the small display holds its own cards, sized Solo' \
    context '[.displayContext.displays[] | select(.name == "Virtual-0" and .holdsCards and .sizing == "solo")] | length == 1'
check 'start: the small display shows one card' small_panes 1
shown=$(small_shows)
waiting=$([[ $shown == "$ta" ]] && echo "$tb" || echo "$ta")
check 'start: its other window waits in Spread for it' waits_for_small "$waiting"
check 'start: the large display shows a card of its own' active_card "$ma"

# Asked for from the dock, the waiting card opens on the small display, its
# own, wherever Spread lives. The large display keeps its card, and the card
# the small display showed waits in Spread for it.
probe activateWindowId "$waiting" >/dev/null
sleep 1.4
report woken
check 'dock: the waiting card opens on the small display, its own' shown_small "$waiting"
check 'dock: the small display still shows one card' small_panes 1
check 'dock: the card it showed waits in Spread for it' waits_for_small "$shown"
check 'dock: the large display keeps its card' active_card "$ma"
read -r shown waiting <<<"$waiting $shown"
own=$ma

# A window opened beside the large display's card makes a layout with it.
open_on_monitor 'Mon C'
mc=$(idOf 'Mon C')
report paired
check 'pair: the large display shows two panes' monitor_panes 2
check 'pair: its own card is a pane there, waiting for no other display' monitor_pane "$own"
check 'pair: the opened window is the other pane' monitor_pane "$mc"
check 'pair: the two fill the display' spans_monitor
check 'pair: both panes have a title bar of KWin'"'"'s' bash -c "for id in $own $mc; do qdbus6 org.kde.KWin /UnloadProbe titleBarOf \$id | jq -e '.height > 0' || exit 1; done"

# Picked in Spread, either pane opens the layout whole, and its partner is
# never left as half a layout.
spread_key
report spread
check 'spread: Spread opens on the large display' presents cardLine
select_card "$own" || echo "could not select $own"
open_selected
report picked
check 'pick: the layout opens with both panes' monitor_panes 2
check 'pick: the picked pane stays on the large display' monitor_pane "$own"
check 'pick: its partner is still a pane' monitor_pane "$mc"
check 'pick: the two fill the display, neither left in half of it' spans_monitor
check 'pick: the small display keeps its card' shown_small "$shown"
check 'pick: the small display still shows one card' small_panes 1

# Clicked on its title bar, then Spread left by Escape, the same.
click_title "$own"
report clicked
check 'click: the clicked pane stays in the layout' monitor_pane "$own"
spread_key
report spread-again
check 'escape: Spread opens on the large display' presents cardLine
escape_key
report escaped
check 'escape: the layout comes back with both panes' monitor_panes 2
check 'escape: the pane stays on the large display' monitor_pane "$own"
check 'escape: the two fill the display' spans_monitor
check 'escape: the small display keeps its card' shown_small "$shown"

# Carried by its title bar to the top edge, over the status bar there, a pane
# is the Active card alone, and its partner a card of its own.
read -r tx _ <<<"$(title_at "$mc")"
title_drag "$mc" "$((tx + 30))" 200 "$tx" 100 "$tx" 40 "$tx" 12 "$tx" 2
report top
check 'top: let go at the top edge over the bar, the pane is the Active card' active_card "$mc"
check 'top: no layout is left on the large display' no_monitor_layout
check 'top: its partner is a card of the large display' monitor_card "$own"
check 'top: the small display keeps its card' shown_small "$shown"
check 'top: every window is a pane or a card' all_held

# Carried by its title bar to the small display, a pane is the card shown
# there. The one pane it leaves is the Active card, not half a layout.
open_on_monitor 'Mon D'
md=$(idOf 'Mon D')
report paired-again
check 'pair again: the large display shows two panes' monitor_panes 2
check 'pair again: the Active card and the opened window are its panes' \
    bash -c "qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -e --arg a '$mc' --arg b '$md' '[.applications[] | select((.windowId == \$a or .windowId == \$b) and .pane and .output == \"Virtual-1\")] | length == 2'"
title_drag "$mc" 2700 400 1100 400 640 360
report carried-small
check 'carry: the carried pane is the card the small display shows' shown_small "$mc"
check 'carry: the small display shows one card' small_panes 1
check 'carry: the card it showed waits in Spread for it' waits_for_small "$shown"
check 'carry: no layout is left on the large display' no_monitor_layout
check 'carry: the pane left behind is the Active card' active_card "$md"
check 'carry: every window is a pane or a card' all_held

# Meta+B pairs the Active card with a card of the large display's own at one
# press. A card waiting there for the small display is never its partner.
probe key 48 125 >/dev/null
sleep 1.6
report bento-key
check 'bento key: one press shows a layout of two' monitor_panes 2
check 'bento key: the Active card is a pane of it' monitor_pane "$md"
check 'bento key: the large display'"'"'s own card is the other' monitor_pane "$own"
check 'bento key: the cards waiting for the small display stay out of it' \
    bash -c "qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -e --arg a '$shown' --arg b '$waiting' '[.applications[] | select((.windowId == \$a or .windowId == \$b) and .hasCard and .stage == \"Virtual-0\")] | length == 2'"
check 'bento key: the small display keeps its card' shown_small "$mc"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .normal and .caption != \"unload-client\")) | length == 5 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display pane: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: the dock opens a card on its own display, a pane of the large display opens its layout whole, its title bar carries it to the top edge or the small display with no half layout left, and Meta+B pairs only the large display'"'"'s own cards'
