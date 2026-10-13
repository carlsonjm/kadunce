#!/usr/bin/env bash
# One Spread, on the display the person chooses. Two plain monitors with no
# touchscreen: by default the largest holds cards, the first on a tie. Chosen
# in kaduncerc, another display takes Spread, and every card goes with it.
# Unplugged, Spread and every card move to the display left; plugged back in,
# Spread returns to the chosen display. At every step exactly one display holds
# cards, every card's window stands on it, and every window there is a card.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: spread display: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { kad workspaceContext | jq -r --arg c "$1" 'first(.applications[] | select(.title == $c)) | .windowId'; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, chosen: .displayContext.spreadDisplay,
        displays: [.displayContext.displays[] | {name, role, geometry}], apps: [.applications[] | {title, output, hasCard, selected}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height})')"
}
# One display holds cards, and it is the one named.
spread_on() {
    context --arg d "$1" '[.displayContext.displays[] | select(.role == "tablet") | .name] == [$d]'
}
# The card and its window name one display, and every window there is a card.
whole() {
    context --arg d "$1" '([.applications[] | select(.hasCard and .output != $d)] | length == 0)
        and ([.applications[] | select(.output == $d and (.hasCard | not) and (.minimized | not))] | length == 0)'
}
cards() { context --argjson n "$1" '[.applications[] | select(.hasCard)] | length == $n'; }
# The Active card's window stands inside the display that holds cards.
active_inside() {
    local id
    id=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
    [[ -n $id && $id != null ]] || return 1
    local display
    display=$(kad workspaceContext | jq -c --arg d "$1" 'first(.displayContext.displays[] | select(.name == $d)) | .geometry')
    probe windowFacts | jq -e --arg id "$id" --argjson g "$display" \
        'first(.[] | select(.id == $id)) // empty
         | .x >= $g.x and .y >= $g.y and .x + .width <= $g.x + $g.width and .y + .height <= $g.y + $g.height'
}
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
unplug() { kscreen-doctor output.Virtual-1.disable >/dev/null; sleep 1.5; }
replug() { kscreen-doctor output.Virtual-1.enable >/dev/null; sleep 1.5; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
# Two windows on the first monitor, one on the second.
client crossCompanion
client ordinaryCompanion
sleep .8
probe sendCaptionToOutput 'unload-client' Virtual-0
probe sendCaptionToOutput 'Ordinary neighbor probe' Virtual-0
probe sendCaptionToOutput 'Cross ownership probe' Virtual-1
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
report start
check 'no touchscreen: one display holds Spread, the first of two the same size' spread_on Virtual-0
check 'no touchscreen: no display was chosen' context '.displayContext.spreadDisplay == ""'
check 'no touchscreen: the windows on that display are its cards, the other display keeps its own' whole Virtual-0
check 'no touchscreen: the two windows there are cards, one of them Active' \
    context '([.applications[] | select(.hasCard)] | length == 2) and .cardStage.presentation == "active"'
check 'no touchscreen: the window on the other display is not a card' \
    context '[.applications[] | select(.title == "Cross ownership probe" and (.hasCard | not))] | length == 1'

kad setSpreadDisplay Virtual-1
sleep 1.5
report chosen
check 'chosen: the chosen display holds Spread, and only it' spread_on Virtual-1
check 'chosen: the choice is kept' context '.displayContext.spreadDisplay == "Virtual-1"'
check 'chosen: the choice is written to kaduncerc' \
    bash -c "kreadconfig6 --file kaduncerc --group Spread --key Display | grep -qx Virtual-1"
check 'chosen: every card moved with Spread, and the window there joined it' whole Virtual-1
check 'chosen: all three windows are cards' cards 3
check 'chosen: the first display holds no card' \
    context '[.applications[] | select(.output == "Virtual-0" and .hasCard)] | length == 0'
check 'chosen: a card is Active' presents active
check 'chosen: the Active card stands on the chosen display' active_inside Virtual-1
kad showCardLine
sleep .8
check 'chosen: Spread opens' presents cardLine
kad showActive
sleep .6

unplug
report unplugged
check 'unplugged: the display left holds Spread, and only it' spread_on Virtual-0
check 'unplugged: the choice is still kept for when it returns' context '.displayContext.spreadDisplay == "Virtual-1"'
check 'unplugged: every card came along, and every window there is a card' whole Virtual-0
check 'unplugged: no card was let go' cards 3
check 'unplugged: a card is Active' presents active
check 'unplugged: the Active card stands on the display left' active_inside Virtual-0
kad showCardLine
sleep .8
check 'unplugged: Spread opens on the display left' presents cardLine
kad showActive
sleep .6
# The person picks another card, as the dock does.
picked=$(idOf 'Ordinary neighbor probe')
probe activateWindowId "$picked"
sleep .8
check 'unplugged: a picked card becomes Active' \
    context --arg id "$picked" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'

replug
report replugged
check 'replugged: Spread returned to the chosen display, and only it' spread_on Virtual-1
check 'replugged: every card went back with it' whole Virtual-1
check 'replugged: no card was let go' cards 3
check 'replugged: the card picked while it was away is still Active' \
    context --arg id "$picked" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
check 'replugged: the Active card stands on the chosen display' active_inside Virtual-1

kad setSpreadDisplay ''
sleep 1.5
report automatic
check 'automatic again: Spread is back on the first display' spread_on Virtual-0
check 'automatic again: the choice is cleared' context '.displayContext.spreadDisplay == ""'
check 'automatic again: every card moved with it' whole Virtual-0
check 'automatic again: no card was let go' cards 3

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\")) | length == 3 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
if (( failures > 0 )); then
    echo "FAIL: spread display: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: one display holds Spread, the chosen one while attached, and every card follows it when it goes and returns'
