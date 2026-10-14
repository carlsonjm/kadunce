#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, on two monitors with no touchscreen:
# Virtual-0 at scale 2 is small and sized Active, Virtual-1 is large, sized
# Connected and chosen for Spread. Every window is a card on both. The small
# display shows one card whole; the rest of its windows wait in Spread on the
# large one, with the card it shows. A card of the small display picked in
# Spread opens there, the card it replaced waits in Spread, and the large
# display goes back to what it showed. A window carried to the small display
# is the card it shows;
# closing it brings back the card it used before. A Stack placed on the small
# display fills it, and Meta+Tab steps it there. With every window moved to
# the small display, Spread still opens, gathering the card it shows, and gives
# it back as it closes. Every check is reported, so
# a failure does not hide the rest.
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
        displays: [.displayContext.displays[] | {name, role, sizing, holdsCards, bentoActive, geometry}],
        apps: [.applications[] | {title, output, hasCard, pane, home, stackId, focused, minimized, spreadRect}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden})')"
}
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
spread_key() { probe key 31 125 >/dev/null; sleep 1; }
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
app() { kad workspaceContext | jq -c --arg id "$1" 'first(.applications[] | select(.windowId == $id))'; }
# A pane of the small display, shown there.
shown_small() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .pane and .output == "Virtual-0" and (.hasCard | not)'; }
# A card waiting in Spread on the large display, to show on the small one.
waits_for_small() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .home == "Virtual-0" and .output == "Virtual-1"'; }
small_panes() { context --argjson n "$1" '[.applications[] | select(.pane and .output == "Virtual-0")] | length == $n'; }
# The one card the small display shows fills it, a gutter inside each edge.
fills_small() {
    local g
    g=$(kad workspaceContext | jq -c 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .geometry')
    probe windowFacts | jq -e --arg id "$1" --argjson g "$g" 'first(.[] | select(.id == $id))
        | .x >= $g.x and .y >= $g.y and .x + .width <= $g.x + $g.width and .y + .height <= $g.y + $g.height
          and .width >= $g.width * 0.8 and .height >= $g.height * 0.7'
}
focused() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .focused'; }
# A card of Spread's row, there on the large display whether or not it is in view.
in_spread() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .output == "Virtual-1"'; }
small_shows() { kad workspaceContext | jq -r 'first(.applications[] | select(.pane and .output == "Virtual-0")) | .windowId'; }
# Spread lives on the large display, right of the small one.
spread_left=2560
# Where a person presses a card of Spread: the middle of the part of it in
# view, or nothing while it is out of view.
seen_at() {
    kad workspaceContext | jq -r --arg id "$1" --argjson left "$spread_left" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | select(. != null) | [([.x, $left] | max), ([.x + .width, $left + 2560] | min)] as [$l, $r]
        | select($r - $l > 120) | "\((($l + $r) / 2) | floor) \((.y + .height / 2) | floor)"'
}
entry_of() { kad workspaceContext | jq -r --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .entry'; }
carry_aim() { kad nativeCarryState | jq -r '"\(.carryAim) \(.carryIndex)"'; }
# A person's finger carries one card onto another in Spread and lets go. Spread
# shows one whole card and its neighbours' edges, so the card is carried as a
# person does: pulled down, the row zooms out. Where all of it shows, the card
# goes straight over the other; where it does not, the card leans the row
# along until the other comes under it, then rests in the middle until it
# would join (CarryRow.h).
carry_onto() {
    local from=$1 to=$2 x y h e entries t items scale width pitch mid x0 dx aim at k
    select_card "$from" >/dev/null 2>&1
    sleep .4
    read -r x y < <(seen_at "$from")
    width=$(kad workspaceContext | jq -r --arg id "$from" 'first(.applications[] | select(.windowId == $id)) | .spreadRect.width')
    h=$(entry_of "$from")
    e=$(entry_of "$to")
    entries=$(kad workspaceContext | jq '[.applications[] | .entry | select(. != null and . >= 0)] | max + 1')
    items=$((entries - 1))
    t=$(( e > h ? e - 1 : e ))
    mid=$((spread_left + 1280))
    probe down 61 "$x" "$y"
    sleep .4
    probe motion 61 "$((x + 30))" "$y"
    sleep .3
    y=$((y + 180))
    probe motion 61 "$((x + 30))" "$y"
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
        # Lean the row along until the held card's middle passes the other.
        probe motion 61 "$(( t >= h ? mid + 600 : mid - 600 ))" "$y"
        for _ in {1..120}; do
            sleep .05
            read -r aim at < <(carry_aim)
            [[ $aim == gap ]] || continue
            if (( t >= h ? at > t : at <= t )); then break; fi
        done
        probe motion 61 "$mid" "$y"
        sleep .6
        x0=$mid
    fi
    for dx in 0 -40 40 -80 80 -120 120 -160 160; do
        probe motion 61 "$((x0 + dx))" "$y"
        sleep .5
        read -r aim at < <(carry_aim)
        [[ $aim == join && $at == "$t" ]] && break
    done
    sleep .4
    echo "state carry-$from $((x0 + dx)) $(kad nativeCarryState | jq -c '{lineCarrying, carryAim, carryIndex, carryScale}')"
    probe up 61
    sleep .8
}
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
check 'start: the large display holds Spread, sized Connected' \
    context '[.displayContext.displays[] | select(.name == "Virtual-1" and .role == "tablet" and .sizing == "connected")] | length == 1'
check 'start: the small display holds its own cards, sized Active' \
    context '[.displayContext.displays[] | select(.name == "Virtual-0" and .holdsCards and .sizing == "active")] | length == 1'
check 'start: no window is an ordinary window' \
    context '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.pane | not))] | length == 0'
check 'start: the small display shows one card' small_panes 1
shown=$(small_shows)
waiting=$([[ $shown == "$ta" ]] && echo "$tb" || echo "$ta")
check 'start: the card it shows fills it' fills_small "$shown"
check 'start: its other window waits in Spread for it' waits_for_small "$waiting"
check 'start: the large display shows a card of its own' \
    context --arg mb "$mb" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $mb'

spread_key
report spread
check 'spread: Spread opens on the large display' presents cardLine
check 'spread: the small display waiting card is in it' in_spread "$waiting"
check 'spread: the card the small display shows is in it too' in_spread "$shown"
pick "$waiting"
report picked
check 'pick: the picked card shows on the small display' shown_small "$waiting"
check 'pick: it fills it' fills_small "$waiting"
check 'pick: the card it replaced waits in Spread for it' waits_for_small "$shown"
check 'pick: the small display still shows one card' small_panes 1
check 'pick: the large display went back to its own card' \
    context --arg mb "$mb" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $mb'
check 'pick: the picked card has focus' focused "$waiting"
# Search floats where the person is working, not on Spread's display.
check 'pick: Search called now floats on the small display' python3 - <<'PY'
import dbus, json
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
reply = json.loads(kadunce.beginLauncherGuest(bus.get_unique_name()))
assert not reply["accepted"] and reply.get("output") == "Virtual-0", reply
PY

# A window carried to the small display is the card it shows.
probe pointer 640 360
probe activateWindowId "$waiting" >/dev/null
sleep .5
client titledCompanion 'Tab C' 500 400
sleep 1.5
tc=$(idOf 'Tab C')
echo "state landed $(probe windowFacts | jq -c --arg id "$tc" 'first(.[] | select(.id == $id)) | {caption, output}')"
if [[ $(app "$tc" | jq -r '.output') != Virtual-0 ]]; then
    probe sendCaptionToOutput 'Tab C' Virtual-0
    sleep 1.2
fi
report opened
check 'open: the new window shows on the small display' shown_small "$tc"
check 'open: the card it showed waits in Spread for it' waits_for_small "$waiting"
check 'open: still one card shown there' small_panes 1

client closeCompanion 'Tab C'
sleep 1.5
report closed
check 'close: the small display shows the card it used before' shown_small "$waiting"
check 'close: still one card shown there' small_panes 1

# Two waiting cards of the small display become one Stack, placed there.
client titledCompanion 'Tab C' 500 400
sleep 1.5
tc=$(idOf 'Tab C')
echo "state landed $(probe windowFacts | jq -c --arg id "$tc" 'first(.[] | select(.id == $id)) | {caption, output}')"
if [[ $(app "$tc" | jq -r '.output') != Virtual-0 ]]; then
    probe sendCaptionToOutput 'Tab C' Virtual-0
    sleep 1.2
fi
report reopened
check 'reopen: the large display shows its own card' presents active
spread_key
report spread-two
carry_onto "$ta" "$tb"
report stacked
check 'stack: the two waiting cards are one Stack' \
    context --arg a "$ta" --arg b "$tb" '[.applications[] | select(.windowId == $a or .windowId == $b) | .stackId] | (length == 2 and (unique | length) == 1)'
open_selected
report stack-placed
face=$(small_shows)
other=$([[ $face == "$ta" ]] && echo "$tb" || echo "$ta")
check 'stack: one of the Stack shows on the small display' bash -c "[[ $face == $ta || $face == $tb ]]"
check 'stack: the rest of it waits for the small display' waits_for_small "$other"
check 'stack: the card it replaced waits in Spread for it' waits_for_small "$tc"
probe activateWindowId "$face" >/dev/null
sleep .5
probe key 15 125 >/dev/null
sleep 1.4
report stepped
check 'step: Meta+Tab shows the next card of the Stack there' shown_small "$other"
check 'step: it fills the small display' fills_small "$other"
check 'step: the card it replaced waits in the Stack' waits_for_small "$face"
check 'step: the large display still shows its own card' presents active

# Every window moved to the small display: Spread's display holds no card of
# its own, and Spread still opens, holding the small display's cards.
probe sendCaptionToOutput 'Mon A' Virtual-0
sleep 1.2
probe sendCaptionToOutput 'Mon B' Virtual-0
sleep 1.5
report emptied
check 'emptied: no card belongs to the large display' \
    context '[.applications[] | select(.title != "unload-client" and .hasCard and .home != "Virtual-0")] | length == 0'
check 'emptied: the small display shows one card' small_panes 1
before=$(small_shows)
spread_key
report gathered
check 'gathered: Spread opens' presents cardLine
check 'gathered: the card the small display showed is in it' in_spread "$before"
check 'gathered: the small display cards wait in it' waits_for_small "$ma"
probe key 1 0 >/dev/null
sleep 1.4
report gave-back
check 'gave back: Spread closed' bash -c "! qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -e '.cardStage.presentation == \"cardLine\"'"
check 'gave back: the small display shows its card again' shown_small "$before"
check 'gave back: still one card shown there' small_panes 1

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 5 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: every window is a card on every display, one Spread, a pick opens on its own display'
