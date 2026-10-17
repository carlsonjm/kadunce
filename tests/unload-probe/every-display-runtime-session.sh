#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, on two monitors with no touchscreen:
# Virtual-0 at scale 2 is small and sized Solo, Virtual-1 is large, sized
# Connected and primary. Each display has its own Spread and its own cards:
# a card belongs to the display its window stands on. The small display shows
# one card whole and its other card waits in its own Spread, never the large
# one's. Spread opens on the display in use and leaves the other as it was;
# both open at once, each names the card in its own middle, and closing one
# leaves the other open. A card picked in the small display's Spread opens
# there. Cards carried together in a display's Spread become a Stack of that
# display, and Meta+Tab steps it there. A window opened on the small display
# is its card; a window sent there from the large display goes with its Stack
# and becomes the card it shows, and the large display keeps showing a card
# of its own. Every check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: every display: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        displays: [.displayContext.displays[] | {name, role, primary, sizing, holdsCards, presentation, shows,
            stage: (.cardStage // {} | {presentation, activeCardId, selectedCardId, spreadOpen, entry: .spreadEntry.cardId})}],
        apps: [.applications[] | {title, output, hasCard, pane, stage, stackId, stackSize, paneStack, focused, minimized, spreadRect}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden})')"
}
small=Virtual-0 large=Virtual-1
# A display's own card stage, as its context line reads it.
stage_of() { kad workspaceContext | jq -c --arg d "$1" 'first(.displayContext.displays[] | select(.name == $d)) | .cardStage'; }
presents_on() { context --arg d "$1" --arg p "$2" 'first(.displayContext.displays[] | select(.name == $d)) | .cardStage.presentation == $p'; }
# The card a display shows on its own, Active.
shows_on() { context --arg d "$1" --arg id "$2" 'first(.displayContext.displays[] | select(.name == $d)) | .cardStage
    | .presentation == "active" and .activeCardId == $id'; }
# A card of the display named: its window stands there and that display's stage holds it.
card_of() { context --arg d "$1" --arg id "$2" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .stage == $d and .output == $d and (.pane | not)'; }
# Every card is a card of the display its window stands on.
each_on_its_own() { context '[.applications[] | select(.hasCard and .stage != .output)] | length == 0'; }
# A card drawn in a display's Spread is drawn inside that display.
drawn_inside() {
    local g
    g=$(kad workspaceContext | jq -c --arg d "$1" 'first(.displayContext.displays[] | select(.name == $d)) | .geometry')
    context --arg id "$2" --argjson g "$g" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | . != null and .x + .width > $g.x and .x < $g.x + $g.width and .y >= $g.y - 1 and .y + .height <= $g.y + $g.height + 1'
}
not_drawn() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .spreadRect == null'; }
# The card in the middle of a display's Spread, which its context line names, is its own.
names_own() { context --arg d "$1" 'first(.displayContext.displays[] | select(.name == $d)) as $own
    | $own.cardStage.spreadEntry.cardId as $id | $id != null
      and (first(.applications[] | select(.windowId == $id)) | .stage == $own.name)'; }
fills() {
    local g
    g=$(kad workspaceContext | jq -c --arg d "$1" 'first(.displayContext.displays[] | select(.name == $d)) | .geometry')
    probe windowFacts | jq -e --arg id "$2" --argjson g "$g" 'first(.[] | select(.id == $id))
        | .x >= $g.x and .y >= $g.y and .x + .width <= $g.x + $g.width and .y + .height <= $g.y + $g.height
          and .width >= $g.width * 0.8 and .height >= $g.height * 0.7'
}
focused() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .focused'; }
active_of() { stage_of "$1" | jq -r '.activeCardId'; }
selected_on() { [[ $(stage_of "$1" | jq -r '.selectedCardId') == "$2" ]]; }
spread_key() { probe key 31 125 >/dev/null; sleep 1; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.4; }
# The arrows page the Spread in use until the card is chosen.
select_on() {
    local d=$1 id=$2 key
    for key in 105 106; do
        for _ in {1..8}; do
            selected_on "$d" "$id" && return 0
            probe key "$key" 0 >/dev/null
            sleep .35
        done
    done
    selected_on "$d" "$id"
}
# Carry the card beside the one in the middle of a display's Spread onto it
# and let go there, as a person makes a Stack (spread-carry.bash, for one
# display among two).
carry_onto_centre_on() {
    local d=$1 id=$2 ctx g centre_id centre_y pick_x centre_x finger target_x held_x
    ctx=$(kad workspaceContext)
    g=$(jq -c --arg d "$d" 'first(.displayContext.displays[] | select(.name == $d)) | .geometry' <<<"$ctx")
    centre_id=$(jq -r --arg d "$d" 'first(.displayContext.displays[] | select(.name == $d)) | .cardStage.selectedCardId' <<<"$ctx")
    read -r centre_x centre_y < <(jq -r --arg id "$centre_id" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"' <<<"$ctx")
    pick_x=$(jq -r --arg d "$d" --arg c "$centre_id" --argjson g "$g" \
        'first(.applications[] | select(.stage == $d and .windowId != $c and .spreadRect)) | .spreadRect
         | (([.x, $g.x] | max) + ([.x + .width, $g.x + $g.width] | min)) / 2 | floor' <<<"$ctx")
    probe down "$id" "$pick_x" "$centre_y"
    sleep .4
    finger=$((pick_x < centre_x ? pick_x + 30 : pick_x - 30))
    probe motion "$id" "$finger" "$centre_y"
    sleep .5
    target_x=$(kad workspaceContext | jq -r --arg id "$centre_id" \
        'first(.applications[] | select(.windowId == $id)) | .spreadRect | (.x + .width / 2) | floor')
    held_x=$(kad nativeCarryState | jq -r '.lineRect | (.x + .width / 2) | floor')
    probe motion "$id" "$((finger + target_x - held_x))" "$centre_y"
    sleep .6
    echo "state carry $(kad nativeCarryState | jq -c '{lineCarrying, carryAim, carryIndex}')"
    probe up "$id"
    sleep 1
}
one_stack() { context --arg a "$1" --arg b "$2" '[.applications[] | select(.windowId == $a or .windowId == $b) | .stackId]
    | (length == 2 and (unique | length) == 1 and .[0] != null)'; }
for attempt in {1..40}; do
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
for title in 'Tab A' 'Tab B' 'Mon A' 'Mon B'; do client titledCompanion "$title" 500 400; sleep .5; done
probe sendCaptionToOutput 'Tab A' Virtual-0
probe sendCaptionToOutput 'Tab B' Virtual-0
probe sendCaptionToOutput 'Mon A' Virtual-1
probe sendCaptionToOutput 'Mon B' Virtual-1
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
ta=$(idOf 'Tab A') tb=$(idOf 'Tab B') ma=$(idOf 'Mon A') mb=$(idOf 'Mon B')
probe activateWindowId "$mb" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
report start
check 'start: the large display is primary, sized Connected' \
    context '[.displayContext.displays[] | select(.name == "Virtual-1" and .primary and .sizing == "connected")] | length == 1'
check 'start: each display has its own card stage' \
    context '[.displayContext.displays[] | select(.holdsCards and .cardStage != null)] | length == 2'
check 'start: no window is an ordinary window' \
    context '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.pane | not))] | length == 0'
check 'start: every card is a card of the display it stands on' each_on_its_own
check 'start: the small display shows a card of its own' presents_on "$small" active
shown=$(active_of "$small")
waiting=$([[ $shown == "$ta" ]] && echo "$tb" || echo "$ta")
check 'start: it shows one of its own two windows' bash -c "[[ $shown == $ta || $shown == $tb ]]"
check 'start: the card it shows fills it' fills "$small" "$shown"
check 'start: its other window is its own card, waiting' card_of "$small" "$waiting"
check 'start: the large display shows a card of its own' shows_on "$large" "$mb"

# Spread opens on the display in use, with that display's cards alone.
probe activateWindowId "$shown" >/dev/null
sleep .5
spread_key
report spread-small
check 'spread: it opens on the small display, the one in use' presents_on "$small" cardLine
check 'spread: the large display still shows its own card' shows_on "$large" "$mb"
check 'spread: the waiting card is drawn on the small display' drawn_inside "$small" "$waiting"
check 'spread: no card of the large display is drawn in it' bash -c "$(declare -f context kad); $(declare -f not_drawn); not_drawn $ma && not_drawn $mb"
check 'spread: the small display line names its own card' names_own "$small"
check 'spread: the context follows the display in use' context '.cardStage.presentation == "cardLine"'
check 'spread: the cards are still each their own display' each_on_its_own
select_on "$small" "$waiting" || echo "could not select $waiting"
open_selected
report picked
check 'pick: the picked card shows on the small display' shows_on "$small" "$waiting"
check 'pick: it fills it' fills "$small" "$waiting"
check 'pick: the card it replaced is still the small display own' card_of "$small" "$shown"
check 'pick: the large display shows the same card' shows_on "$large" "$mb"
check 'pick: the picked card has focus' focused "$waiting"

# Both Spreads open at once, as each display's dock opens its own.
kad showCardLineOn "$large" >/dev/null
sleep 1.2
report spread-large
check 'two: the large display opens its own Spread' presents_on "$large" cardLine
check 'two: the small display keeps its card' shows_on "$small" "$waiting"
check 'two: the large display Spread holds its own cards' drawn_inside "$large" "$ma"
check 'two: and none of the small display' not_drawn "$shown"
kad showCardLineOn "$small" >/dev/null
sleep 1.2
report spread-both
check 'two: both Spreads are open' bash -c "$(declare -f context kad presents_on); presents_on $small cardLine && presents_on $large cardLine"
check 'two: the small display line names its own card' names_own "$small"
check 'two: the large display line names its own card' names_own "$large"
check 'two: each Spread draws its own cards inside its display' \
    bash -c "$(declare -f context kad drawn_inside); drawn_inside $small $shown && drawn_inside $large $mb"
kad showActiveOn "$small" >/dev/null
sleep 1.2
report closed-small
check 'two: closing the small display Spread leaves the large one open' presents_on "$large" cardLine
check 'two: the small display shows its card again' presents_on "$small" active
kad showActiveOn "$large" >/dev/null
sleep 1.2
report closed-large
check 'two: closed, the large display shows a card of its own' presents_on "$large" active

# A Stack of the large display, made in its Spread, steps there.
probe activateWindowId "$mb" >/dev/null
sleep .5
spread_key
report spread-stack
check 'stack: Spread opens on the large display, in use' presents_on "$large" cardLine
carry_onto_centre_on "$large" 71
report stacked
check 'stack: the two cards of the large display are one Stack' one_stack "$ma" "$mb"
check 'stack: the Stack is the large display own' bash -c "$(declare -f context kad card_of); card_of $large $ma && card_of $large $mb"
check 'stack: the small display is untouched' presents_on "$small" active
open_selected
report stack-shown
face=$(active_of "$large")
behind=$([[ $face == "$ma" ]] && echo "$mb" || echo "$ma")
check 'stack: one of the Stack shows on the large display' bash -c "[[ $face == $ma || $face == $mb ]]"
probe activateWindowId "$face" >/dev/null
sleep .5
probe key 15 125 >/dev/null
sleep 1.4
report stepped
check 'step: Meta+Tab shows the next card of the Stack there' shows_on "$large" "$behind"
check 'step: the small display still shows its card' shows_on "$small" "$waiting"

# A window opened on the small display is its card.
probe pointer 640 360
probe activateWindowId "$waiting" >/dev/null
sleep .5
client titledCompanion 'Tab C' 500 400
sleep 1.5
tc=$(idOf 'Tab C')
echo "state landed $(probe windowFacts | jq -c --arg id "$tc" 'first(.[] | select(.id == $id)) | {caption, output}')"
if [[ $(kad workspaceContext | jq -r --arg id "$tc" 'first(.applications[] | select(.windowId == $id)) | .output') != "$small" ]]; then
    probe sendCaptionToOutput 'Tab C' Virtual-0
    sleep 1.2
fi
report opened
check 'open: the new window is a card of the small display' card_of "$small" "$tc"
check 'open: it shows there' shows_on "$small" "$tc"
check 'open: the large display shows the same card' presents_on "$large" active
client closeCompanion 'Tab C'
sleep 1.5
report closed
check 'close: the small display shows a card of its own again' presents_on "$small" active
check 'close: every card is a card of the display it stands on' each_on_its_own

# A window sent from the large display goes with its Stack and is the card
# the small display shows.
sent=$(active_of "$large")
other=$([[ $sent == "$ma" ]] && echo "$mb" || echo "$ma")
caption=$([[ $sent == "$ma" ]] && echo 'Mon A' || echo 'Mon B')
probe sendCaptionToOutput "$caption" Virtual-0
sleep 2
report sent
check 'sent: the sent card is now a card of the small display' card_of "$small" "$sent"
check 'sent: its Stack went with it' card_of "$small" "$other"
check 'sent: still one Stack' one_stack "$ma" "$mb"
check 'sent: the small display shows it' shows_on "$small" "$sent"
check 'sent: every card is a card of the display it stands on' each_on_its_own
probe activateWindowId "$sent" >/dev/null
sleep .5
spread_key
report sent-spread
check 'sent: the small display Spread holds what it was given' drawn_inside "$small" "$shown"
check 'sent: the large display Spread stays shut' bash -c "$(declare -f context kad presents_on); ! presents_on $large cardLine"
probe key 1 0 >/dev/null
sleep 1.2

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 4 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: every display has its own Spread and its own cards'
