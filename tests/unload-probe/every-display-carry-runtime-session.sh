#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, as cards leave a tablet for a monitor:
# Virtual-0 at scale 2 is small, sized Solo and chosen for Spread, and
# Virtual-1 is large, sized Connected and shows a layout of its own two
# windows, each given KWin's own title bar. The small display's Active card,
# carried by that title bar and rested on a monitor window, joins the layout
# as a pane of its own: no window there is taken into a Stack, none is drawn
# over another, the window it rested on stays in the layout though it stops
# short of its new pane, and the small display keeps its other card. Spread
# then moves to the monitor, as choosing it in Settings does, while the small
# display shows a card: that card stays there as the small display's own, its
# waiting card goes with Spread, and the monitor's windows keep their places,
# with nothing drawn over them. Asked for from the dock, that waiting card
# opens on the small display, its own, and stays there though it stops short
# of its place, as a terminal keeping whole rows does. Carried off from there
# onto the monitor, it leaves the small display the card it showed before,
# never nothing. A card let out of a Stack on the small display, with Spread
# on the monitor, joins the monitor's layout; one pulled out of that Stack in
# Spread joins it once picked. Every check is reported, so a failure does not
# hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: every display carry: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        displays: [.displayContext.displays[] | {name, role, sizing, holdsCards}],
        apps: [.applications[] | {title, output, hasCard, pane, paneStack, stackSize, home, focused, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden, minimized})')"
    echo "stages $1 $(kad outputStageState | tr '\n' ' ')"
}
holds_spread() { context --arg d "$1" '[.displayContext.displays[] | select(.name == $d and .role == "tablet")] | length == 1'; }
# Spread's display shows this card, Active.
keeps_own() { context --arg id "$1" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'; }
# A window on that display, shown there, held by no card.
shown_on() { context --arg id "$1" --arg d "$2" 'first(.applications[] | select(.windowId == $id))
    | .output == $d and (.hasCard | not) and (.minimized | not)'; }
monitor_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .pane and .output == "Virtual-1" and (.hasCard | not) and (.minimized | not)'; }
monitor_panes() { context --argjson n "$1" '[.applications[] | select(.pane and .output == "Virtual-1")] | length == $n'; }
# A card waiting in Spread, awake, for the display named, or Spread's own.
waits_for() { context --arg id "$1" --arg d "$2" 'first(.applications[] | select(.windowId == $id))
    | .hasCard and (.minimized | not) and .home == $d'; }
# No window holds another behind it, as a card's Stack or a pane's.
no_stack() { context '[.applications[] | select((.stackSize // 1) > 1 or ((.paneStack // []) | length) > 0)] | length == 0'; }
# No two windows a person sees on a display stand over each other, which is
# how cards piled in one place look like a Stack that is none. On Spread's
# display a card is seen only while it is the Active card; on another
# display, a card standing there is drawn where it stands.
apart() {
    local facts
    facts=$(probe windowFacts) || return 1
    context --arg d "$1" --argjson facts "$facts" '
        (first(.displayContext.displays[] | select(.role == "tablet")) | .name) as $spread
        | .cardStage as $stage
        | [.applications[] | select(.minimized | not)
            | select(.pane or (.hasCard | not) or $d != $spread
                or ($stage.presentation == "active" and .windowId == $stage.selectedCardId))
            | .windowId] as $seen
        | [$facts[] | select(.output == $d and (.hidden | not) and (.minimized | not))
            | select(.id as $id | any($seen[]; . == $id))] as $drawn
        | [range(0; $drawn | length) as $i | range($i + 1; $drawn | length) as $j
            | select(([$drawn[$i].x + $drawn[$i].width, $drawn[$j].x + $drawn[$j].width] | min)
                    - ([$drawn[$i].x, $drawn[$j].x] | max) > 2
                and ([$drawn[$i].y + $drawn[$i].height, $drawn[$j].y + $drawn[$j].height] | min)
                    - ([$drawn[$i].y, $drawn[$j].y] | max) > 2)]
        | length == 0'
}
frames_of() {
    probe windowFacts | jq -c --arg a "$1" --arg b "$2" --arg c "$3" \
        '[.[] | select(.id == $a or .id == $b or .id == $c) | {id, x, y, width, height}]'
}
# The first window stands shorter than the second, by more than a pixel's rounding.
shorter() {
    local a b
    a=$(probe windowGeometry "$1" | jq '.height | floor') || return 1
    b=$(probe windowGeometry "$2" | jq '.height | floor') || return 1
    (( a < b - 12 ))
}
# Each window still stands where it stood, to a few pixels.
stand_as() {
    probe windowFacts | jq -e --argjson was "$1" '. as $now | ($was | length) == 3 and all($was[]; . as $w
        | first($now[] | select(.id == $w.id)) as $n
        | [$n.x - $w.x, $n.y - $w.y, $n.width - $w.width, $n.height - $w.height]
        | map(if . < 0 then -. else . end) | max <= 4)'
}
# A window carried by KWin's own title bar, as a person's mouse does, rested
# on another window past the time a pane needs to join a Stack there, and
# let go.
# Let go a moment after reaching the window, sooner than a rest that stacks;
# a third argument rests that long first.
spread_key() { probe key 31 125 >/dev/null; sleep 1.2; }
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
title_carry() {
    local id=$1 onto=$2 rest=${3:-.2} x y tx ty
    read -r x y <<<"$(probe titleBarOf "$id" | jq -r '[.x, .y] | map(floor) | @tsv')"
    read -r tx ty <<<"$(probe windowGeometry "$onto" | jq -r '[.x + .width / 2, .y + .height / 2] | map(floor) | @tsv')"
    echo "carry from $x,$y onto $tx,$ty"
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe contactButton true >/dev/null
    sleep .15
    probe contactMotion "$((x + 40))" "$((y + 20))" >/dev/null
    sleep .1
    probe contactMotion "$(((x + tx) / 2))" "$(((y + ty) / 2))" >/dev/null
    sleep .1
    probe contactMotion "$tx" "$ty" >/dev/null
    sleep "$rest"
    kad nativeCarryState >"$XDG_RUNTIME_DIR/carry"
    echo "state resting $(jq -c '{destination, destinationRect, stackOnto}' "$XDG_RUNTIME_DIR/carry")"
    echo "state trace $(kad nativeMoveTrace | jq -Rsc 'split("\n") | map(select(length > 0)) | .[-6:]')"
    probe contactButton false >/dev/null
    sleep 1.6
}
for _ in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
kscreen-doctor output.Virtual-0.scale.2 >/dev/null 2>&1
sleep 1
echo "displays: $(kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Output|Geometry|Scale' | tr -s ' \n' ' ')"
kwriteconfig6 --file kaduncerc --group Spread --key Display Virtual-0
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
for id in "$ta" "$tb" "$ma" "$mb"; do probe serverDecorate "$id" >/dev/null; done
sleep .4
probe activateWindowId "$tb" >/dev/null
sleep .3
probe activateWindowId "$ta" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
report start
check 'start: the small display holds Spread' holds_spread Virtual-0
check 'start: the monitor holds its own cards, sized Connected' \
    context '[.displayContext.displays[] | select(.name == "Virtual-1" and .holdsCards and .sizing == "connected")] | length == 1'
check 'start: the monitor shows its two windows as a layout' monitor_panes 2
check 'start: the small display shows its own card' keeps_own "$ta"
check 'start: its other card waits in Spread' waits_for "$tb" ''

# The small display's card, carried by its title bar onto a monitor window.
# The window it rests on keeps whole rows, as a terminal does, so it stops
# short of the pane the layout of three gives it.
client companionRows 'Mon B' 48
title_carry "$ta" "$mb"
report carried
check 'carry: let go at once on a monitor window, it aims at no Stack' \
    jq -e '(.stackOnto // "") == ""' "$XDG_RUNTIME_DIR/carry"
check 'carry: the card carried is a monitor pane, held by no card' monitor_pane "$ta"
check 'carry: the window it rested on is still a monitor pane' monitor_pane "$mb"
check 'carry: the other monitor window is still a monitor pane' monitor_pane "$ma"
check 'carry: the monitor shows its three windows as a layout' monitor_panes 3
check 'carry: no window holds another as a Stack' no_stack
check 'carry: no window on the monitor stands over another' apart Virtual-1
check 'carry: the small display still holds its other card' \
    context --arg id "$tb" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .output == "Virtual-0"'
check 'carry: the window short of its pane stays in the layout, short of it' shorter "$mb" "$ma"
client companionRows 'Mon B' 0

# A new card on the small display, in front, with the other waiting behind.
probe pointer 640 360 >/dev/null
client titledCompanion 'Tab C' 500 400
sleep 1.4
tc=$(idOf 'Tab C')
probe activateWindowId "$tc" >/dev/null
sleep 1
before=$(frames_of "$ta" "$ma" "$mb")
report before-move
check 'before the move: the small display shows its new card' keeps_own "$tc"
check 'before the move: its other card waits in Spread' waits_for "$tb" ''

# Spread moves to the monitor, as choosing it in Settings does.
kad setSpreadDisplay Virtual-1
sleep 2.5
report moved
check 'move: the monitor holds Spread' holds_spread Virtual-1
check 'move: the small display keeps the card it showed, as its own' shown_on "$tc" Virtual-0
check 'move: the card waiting there goes with Spread, for the small display' waits_for "$tb" Virtual-0
check 'move: the monitor still shows its three windows as a layout' monitor_panes 3
check 'move: the monitor windows stand where they stood' stand_as "$before"
check 'move: no window on the monitor stands over another' apart Virtual-1
check 'move: no window on the small display stands over another' apart Virtual-0
check 'move: no window holds another as a Stack' no_stack

# Asked for from the dock, as a person taps it there, the card waiting for the
# small display opens there, its own, though Spread lives on the monitor. Like
# a terminal keeping whole rows, it stops short of its place there, and it
# stays: alone on that display it overlaps nothing.
place=$(probe windowGeometry "$tc" | jq '.height | floor')
client companionRows 'Tab B' 48
probe activateWindowId "$tb" >/dev/null
sleep 1.4
report dock
check 'dock: the small display'"'"'s waiting card opens there' shown_on "$tb" Virtual-0
check 'dock: the card it showed waits in Spread for it' waits_for "$tc" Virtual-0
check 'dock: the monitor still shows its three windows as a layout' monitor_panes 3
check 'dock: no window on the small display stands over another' apart Virtual-0
sleep 1.5
report dock-settled
check 'dock: the card stops short of its place' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowGeometry '$tb' | jq -e --argjson p '$place' '.height < \$p - 12'"
check 'dock: and still stands on the small display once it has settled' shown_on "$tb" Virtual-0
client companionRows 'Tab B' 0

# Carried off by its title bar onto the monitor, the small display's card is
# the monitor's own, and the small display shows the card it showed before.
title_carry "$tb" "$mb"
report carried-off
check 'carry off: the card carried is on the monitor, its own' \
    context --arg id "$tb" 'first(.applications[] | select(.windowId == $id)) | .output == "Virtual-1" and .home == ""'
check 'carry off: the small display shows the card it showed before' shown_on "$tc" Virtual-0
check 'carry off: no window on the small display stands over another' apart Virtual-0
check 'carry off: no window holds another as a Stack' no_stack

# Spread comes back to the small display; the monitor keeps its layout.
kad setSpreadDisplay Virtual-0
sleep 2.5
report back
check 'back: the small display holds Spread again' holds_spread Virtual-0
check 'back: the monitor still shows a layout' \
    context '[.applications[] | select(.pane and .output == "Virtual-1")] | length >= 3'
check 'back: the small display keeps its card' \
    context --arg c "$tc" 'first(.applications[] | select(.windowId == $c)) | .output == "Virtual-0"'
check 'back: the card carried to the monitor stays there' \
    context --arg b "$tb" 'first(.applications[] | select(.windowId == $b)) | .output == "Virtual-1"'
check 'back: no window on the monitor stands over another' apart Virtual-1
check 'back: no window holds another as a Stack' no_stack

# Carried onto a monitor pane and rested there, the small display's card
# joins that pane's Stack in front of it, as a pane rested on another does.
probe serverDecorate "$tc" >/dev/null
sleep .4
title_carry "$tc" "$mb" 1.0
report rested
check 'rest: the carry names the pane it would join' \
    jq -e --arg id "$mb" '.stackOnto == $id' "$XDG_RUNTIME_DIR/carry"
sleep 1
report stacked
check 'rest: the card carried is a monitor pane' monitor_pane "$tc"
check 'rest: the pane it rested on waits behind it' \
    context --arg c "$tc" --arg m "$mb" 'first(.applications[] | select(.windowId == $c)) | (.paneStack // []) | index($m) != null'
check 'rest: no window on the monitor stands over another' apart Virtual-1

# With Spread on the monitor, as on the tablet with a monitor beside it, a
# monitor pane rested on the card the small display shows alone joins it,
# though the small display is sized Solo; let out of that Stack by the keys
# it joins Spread's layout, and rested on a pane there, it joins that pane.
probe pointer 640 360 >/dev/null
client titledCompanion 'Tab D' 500 400
sleep 1.4
td=$(idOf 'Tab D')
probe serverDecorate "$td" >/dev/null
probe activateWindowId "$td" >/dev/null
sleep 1
kad setSpreadDisplay Virtual-1
sleep 2.5
report spread-on-monitor
check 'monitor Spread: the small display shows its card alone' shown_on "$td" Virtual-0
title_carry "$ma" "$td" 1.0
report rested-on-small
check 'small rest: the carry names the card it would join' \
    jq -e --arg id "$td" '.stackOnto == $id' "$XDG_RUNTIME_DIR/carry"
sleep 1
check 'small rest: the pane carried stands on the small display' \
    context --arg c "$ma" 'first(.applications[] | select(.windowId == $c)) | .output == "Virtual-0"'
check 'small rest: the card it rested on waits behind it' \
    context --arg c "$ma" --arg t "$td" 'first(.applications[] | select(.windowId == $c)) | (.paneStack // []) | index($t) != null'
# Let out of that Stack by the keys, it has no room for a pane beside the
# card on the small display, so it joins Spread's layout as a pane of its
# own, and the card behind it is shown alone again.
probe activateWindowId "$ma" >/dev/null
sleep .6
probe keyHeld 108 1 125 42 >/dev/null
sleep 1.8
report unstacked
check 'unstack: the card let out is a pane of the layout Spread shows' monitor_pane "$ma"
check 'unstack: the small display shows the card behind it alone' shown_on "$td" Virtual-0
check 'unstack: no window on the monitor stands over another' apart Virtual-1
title_carry "$ma" "$ta" 1.0
report rested-on-spread
check 'Spread rest: the carry names the pane it would join' \
    jq -e --arg id "$ta" '.stackOnto == $id' "$XDG_RUNTIME_DIR/carry"
sleep 1
check 'Spread rest: the card carried is a monitor pane' monitor_pane "$ma"
check 'Spread rest: the pane it rested on waits behind it' \
    context --arg c "$ma" --arg t "$ta" 'first(.applications[] | select(.windowId == $c)) | (.paneStack // []) | index($t) != null'

# Rested on the small display's card again, it brings the Stack it holds.
# Pulled out of that Stack in Spread on the monitor, a card waits for
# Spread's display rather than the small one, and picked it joins the layout
# there as a pane of its own.
title_carry "$ma" "$td" 1.0
sleep 1
report rested-again
check 'again: the card it rested on waits behind it' \
    context --arg c "$ma" --arg t "$td" 'first(.applications[] | select(.windowId == $c)) | (.paneStack // []) | index($t) != null'
spread_key
select_card "$td" || echo "could not select $td"
probe keyHeld 108 1 125 42 >/dev/null
sleep 1.4
report pulled
check 'pull: the card pulled out waits for Spread' waits_for "$td" ''
check 'pull: it is no longer in the Stack' \
    context --arg c "$ma" --arg t "$td" 'first(.applications[] | select(.windowId == $c)) | (.paneStack // []) | index($t) == null'
select_card "$td" || echo "could not select $td"
probe key 28 0 >/dev/null
sleep 1.6
report pulled-picked
check 'pull: picked, it is a pane of the layout Spread shows' monitor_pane "$td"
check 'pull: no window on the monitor stands over another' apart Virtual-1

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 6 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display carry: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: a card carried to the monitor or left behind by Spread stands on its own there, the dock opens a card on its own display, a display a card is carried off keeps one, and no display piles its windows into a Stack'
