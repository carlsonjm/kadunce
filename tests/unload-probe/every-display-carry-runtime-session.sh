#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, as cards leave a tablet for a monitor:
# Virtual-0 at scale 2 is small, sized Solo and primary, and Virtual-1 is
# large, sized Connected and shows a layout of its own two windows, each given
# KWin's own title bar. Each display has its own Spread and its own cards. The
# small display's Active card, carried by that title bar and let go on a
# monitor window, joins the layout as a pane of its own: no window is taken
# into a Stack, none is drawn over another, the window it was let go on stays
# though it stops short of its new pane, and the small display shows its
# other card. Choosing the monitor as primary moves no card. Asked for from the
# dock, a card waiting in the small display's Spread opens there and stays
# though it stops short of its place. Carried off onto the monitor, it leaves
# the small display the card it showed before. A card rested on a monitor pane
# joins that pane's Stack; a monitor pane rested on the small display's card
# joins its Stack there; a Stack carried from the monitor goes whole and steps
# there by Meta+Tab. Let go at the small display's bottom edge, its card is an
# ordinary window. Every check is reported, so a failure does not hide the rest.
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
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation,
        displays: [.displayContext.displays[] | {name, role, primary, sizing, holdsCards, shows,
            s: .cardStage.presentation, a: .cardStage.activeCardId}],
        apps: [.applications[] | {title, output, hasCard, pane, stage, paneStack, stackId, stackSize, focused, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden, minimized})')"
    echo "stages $1 $(kad outputStageState | tr '\n' ' ')"
}
is_primary() { context --arg d "$1" '[.displayContext.displays[] | select(.name == $d and .primary)] | length == 1'; }
# A display shows this card, Active, on its own stage.
shown_on() { context --arg id "$1" --arg d "$2" '(first(.displayContext.displays[] | select(.name == $d)) | .cardStage
    | .presentation == "active" and .activeCardId == $id)
    and (first(.applications[] | select(.windowId == $id)) | .output == $d and .stage == $d and (.minimized | not))'; }
keeps_own() { shown_on "$1" Virtual-0; }
monitor_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .pane and .output == "Virtual-1" and (.hasCard | not) and (.minimized | not)'; }
monitor_panes() { context --argjson n "$1" '[.applications[] | select(.pane and .output == "Virtual-1")] | length == $n'; }
# A card of the display named, awake, waiting in that display's own Spread.
waits_on() { context --arg id "$1" --arg d "$2" '(first(.displayContext.displays[] | select(.name == $d)) | .cardStage.activeCardId != $id)
    and (first(.applications[] | select(.windowId == $id)) | .hasCard and .stage == $d and .output == $d and (.minimized | not))'; }
# No window holds another behind it, as a card's Stack or a pane's.
no_stack() { context '[.applications[] | select((.stackSize // 1) > 1 or ((.paneStack // []) | length) > 0)] | length == 0'; }
one_stack() { context --arg a "$1" --arg b "$2" '[.applications[] | select(.windowId == $a or .windowId == $b) | .stackId]
    | (length == 2 and (unique | length) == 1 and .[0] != null)'; }
# No two windows a person sees on a display stand over each other, which is
# how cards piled in one place look like a Stack that is none. A card is seen
# only while it is its display's Active card; a pane or a window always.
apart() {
    local facts
    facts=$(probe windowFacts) || return 1
    context --arg d "$1" --argjson facts "$facts" '
        (first(.displayContext.displays[] | select(.name == $d)) | .cardStage // {}) as $stage
        | [.applications[] | select(.minimized | not)
            | select(.pane or (.hasCard | not)
                or ($stage.presentation == "active" and .windowId == $stage.activeCardId))
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
# The monitor shows its own Active card at first, as every display does;
# Meta+B there lays its windows out, as a person does, and the small display
# is in use again.
lay_out_monitor() {
    probe activateWindowId "$1" >/dev/null
    sleep .6
    probe key 48 125 >/dev/null
    sleep 1.6
    probe activateWindowId "$2" >/dev/null
    sleep .8
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
lay_out_monitor "$mb" "$ta"
report start
check 'start: the small display is primary' is_primary Virtual-0
check 'start: the monitor holds its own cards, sized Connected' \
    context '[.displayContext.displays[] | select(.name == "Virtual-1" and .holdsCards and .sizing == "connected")] | length == 1'
check 'start: the monitor shows its two windows as a layout' monitor_panes 2
check 'start: the small display shows its own card' keeps_own "$ta"
check 'start: its other card waits in its own Spread' waits_on "$tb" Virtual-0

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
check 'carry: the small display shows its other card' keeps_own "$tb"
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
report before-primary
check 'new: the small display shows its new card' keeps_own "$tc"
check 'new: its other card waits in its own Spread' waits_on "$tb" Virtual-0

# The monitor chosen as primary, as Settings does: no card moves.
kad setSpreadDisplay Virtual-1
sleep 2.5
report primary
check 'primary: the monitor is primary' is_primary Virtual-1
check 'primary: the small display keeps the card it showed' keeps_own "$tc"
check 'primary: its waiting card stays its own' waits_on "$tb" Virtual-0
check 'primary: the monitor still shows its three windows as a layout' monitor_panes 3
check 'primary: the monitor windows stand where they stood' stand_as "$before"
check 'primary: no window on the monitor stands over another' apart Virtual-1
check 'primary: no window on the small display stands over another' apart Virtual-0
check 'primary: no window holds another as a Stack' no_stack

# Asked for from the dock, as a person taps it there, the small display's
# waiting card opens there. Like a terminal keeping whole rows, it stops short
# of its place, and it stays: alone on that display it overlaps nothing.
place=$(probe windowGeometry "$tc" | jq '.height | floor')
client companionRows 'Tab B' 48
probe activateWindowId "$tb" >/dev/null
sleep 1.4
report dock
check 'dock: the small display waiting card opens there' keeps_own "$tb"
check 'dock: the card it showed waits in its Spread' waits_on "$tc" Virtual-0
check 'dock: the monitor still shows its three windows as a layout' monitor_panes 3
check 'dock: no window on the small display stands over another' apart Virtual-0
sleep 1.5
report dock-settled
check 'dock: the card stops short of its place' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowGeometry '$tb' | jq -e --argjson p '$place' '.height < \$p - 12'"
check 'dock: and still stands on the small display once it has settled' keeps_own "$tb"
client companionRows 'Tab B' 0

# Carried off by its title bar onto the monitor, the small display's card is
# the monitor's, and the small display shows the card it showed before.
title_carry "$tb" "$mb"
report carried-off
check 'carry off: the card carried is on the monitor, its own' \
    context --arg id "$tb" 'first(.applications[] | select(.windowId == $id)) | .output == "Virtual-1" and (.stage // "Virtual-1") == "Virtual-1"'
check 'carry off: the small display shows the card it showed before' keeps_own "$tc"
check 'carry off: no window on the small display stands over another' apart Virtual-0
check 'carry off: no window holds another as a Stack' no_stack
kad setSpreadDisplay Virtual-0
sleep 1.5

# Carried onto a monitor pane and rested there, the small display's card
# joins that pane's Stack in front of it, as a pane rested on another does.
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

# A monitor pane rested on the card the small display shows joins its Stack
# there, on the small display's own stage.
probe pointer 640 360 >/dev/null
client titledCompanion 'Tab D' 500 400
sleep 1.4
td=$(idOf 'Tab D')
probe serverDecorate "$td" >/dev/null
probe activateWindowId "$td" >/dev/null
sleep 1
report tab-d
check 'small: the small display shows its new card' keeps_own "$td"
title_carry "$ma" "$td" 1.0
report rested-on-small
check 'small rest: the carry names the card it would join' \
    jq -e --arg id "$td" '.stackOnto == $id' "$XDG_RUNTIME_DIR/carry"
sleep 1
report stacked-on-small
check 'small rest: the two are one Stack' one_stack "$ma" "$td"
check 'small rest: the Stack is the small display own' \
    context --arg a "$ma" --arg t "$td" '[.applications[] | select(.windowId == $a or .windowId == $t) | .stage] | all(. == "Virtual-0")'
check 'small rest: the small display shows a card of it' \
    context --arg a "$ma" --arg t "$td" 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .cardStage.activeCardId as $s | $s == $a or $s == $t'
check 'small rest: the monitor keeps its layout' \
    context '[.applications[] | select(.pane and .output == "Virtual-1")] | length >= 2'

# A Stack made on the monitor, carried by its front pane's title bar to the
# small display and let go there at once, goes there whole, and Meta+Tab
# steps it there.
report stack-before
check 'stack: a monitor pane holds a Stack' \
    context --arg c "$tc" --arg m "$mb" 'first(.applications[] | select(.windowId == $c)) | .pane and .output == "Virtual-1" and ((.paneStack // []) | index($m) != null)'
small_card=$(kad workspaceContext | jq -r 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .cardStage.activeCardId')
title_carry "$tc" "$small_card"
sleep 1
report stack-carried
check 'stack carry: the pane carried is the card the small display shows' keeps_own "$tc"
check 'stack carry: its Stack came with it' one_stack "$tc" "$mb"
check 'stack carry: the card behind is the small display own' \
    context --arg m "$mb" 'first(.applications[] | select(.windowId == $m)) | .hasCard and .stage == "Virtual-0" and .output == "Virtual-0"'
check 'stack carry: no window on the small display stands over another' apart Virtual-0
probe activateWindowId "$tc" >/dev/null
sleep .5
probe key 15 125 >/dev/null
sleep 1.4
report stack-stepped
check 'step: Meta+Tab shows the card behind' keeps_own "$mb"
probe key 15 125 >/dev/null
sleep 1.4
report stack-stepped-back
check 'step: again, it comes round to the front' keeps_own "$tc"
check 'step: no window on the small display stands over another' apart Virtual-0

# Carried by its title bar to the small display's bottom edge and let go
# there, the card that display shows goes to its ordinary desktop (§10).
read -r x y <<<"$(probe titleBarOf "$tc" | jq -r '[.x, .y] | map(floor) | @tsv')"
read -r bx by <<<"$(kad workspaceContext | jq -r 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .geometry | [.x + .width / 2, .y + .height - 4] | map(floor) | @tsv')"
echo "bottom carry from $x,$y to $bx,$by"
probe pointer "$x" "$y" >/dev/null
sleep .1
probe contactButton true >/dev/null
sleep .15
probe contactMotion "$((x + 20))" "$((y + 40))" >/dev/null
sleep .1
probe contactMotion "$bx" "$(((y + by) / 2))" >/dev/null
sleep .1
probe contactMotion "$bx" "$by" >/dev/null
sleep .3
echo "state at the bottom edge $(kad nativeCarryState | jq -c '{destination, destinationRect}')"
probe contactButton false >/dev/null
sleep 1.6
report bottom-released
check 'bottom: the card let go at the bottom edge is no card' \
    context --arg id "$tc" 'first(.applications[] | select(.windowId == $id)) | (.hasCard | not) and (.pane | not)'
check 'bottom: it stays on the small display, shown' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e --arg c 'Tab C' 'first(.[] | select(.caption == \$c)) | .output == \"Virtual-0\" and (.hidden | not) and (.minimized | not)'"

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
echo 'PASS: cards carried between displays join the stage of the display they land on, with their Stacks, and no display piles its windows into a Stack'
