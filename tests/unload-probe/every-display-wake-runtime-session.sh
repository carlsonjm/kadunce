#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, as a tablet with a monitor is used:
# Virtual-0 at scale 2 is small, sized Solo and primary, and
# Virtual-1 is large, sized Connected and shows a layout of its own two
# windows, which Spread leaves where they are. A monitor window minimized
# from the dock waits asleep in the monitor's own Spread; brought back from the dock
# it is the monitor's Active card, and clicked there each window is in use on
# the monitor, while the small display keeps its own card, never opening Spread. Picked asleep in
# the monitor's Spread, it wakes there as its Active card. Every check is reported, so a failure
# does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: every display wake: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        displays: [.displayContext.displays[] | {name, role, sizing, holdsCards, bentoActive}],
        apps: [.applications[] | {title, output, hasCard, pane, stage, focused, minimized}],
        stages: [.displayContext.displays[] | {name, shows, s: .cardStage.presentation, a: .cardStage.activeCardId}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden, minimized})')"
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
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
# The small display shows its own card, Active, and no Spread.
shows_on_monitor() { context --arg id "$1" 'first(.displayContext.displays[] | select(.name == "Virtual-1")) | .cardStage | .presentation == "active" and .activeCardId == $id'; }
keeps_own() { context --arg id "$1" 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .cardStage | .presentation == "active" and .activeCardId == $id'; }
presents_on() { context --arg d "$1" --arg p "$2" 'first(.displayContext.displays[] | select(.name == $d)) | .cardStage.presentation == $p'; }
# A pane of the monitor's layout, shown there, held by no card.
monitor_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .pane and .output == "Virtual-1" and (.hasCard | not) and (.minimized | not)'; }
monitor_panes() { context --argjson n "$1" '[.applications[] | select(.pane and .output == "Virtual-1")] | length == $n'; }
# A card of the monitor waiting asleep in the monitor's own Spread.
sleeps_for_monitor() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .hasCard and .stage == "Virtual-1" and .minimized'; }
focused() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .focused'; }
# A person's click in the middle of a window, with the pointer.
click_on() {
    local x y
    read -r x y <<<"$(probe windowGeometry "$1" | jq -r '[.x + .width / 2, .y + .height / 2] | map(floor) | @tsv')"
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe button true >/dev/null
    sleep .05
    probe button false >/dev/null
    sleep 1.2
}
# A person's click on a window's title bar, KWin's own.
click_title() {
    local x y
    read -r x y <<<"$(probe titleBarOf "$1" | jq -r '[.x, .y] | map(floor) | @tsv')"
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe button true >/dev/null
    sleep .05
    probe button false >/dev/null
    sleep 1.2
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
for title in 'Tab' 'Mon A' 'Mon B'; do client titledCompanion "$title" 500 400; sleep .5; done
probe sendCaptionToOutput 'Tab' Virtual-0
probe sendCaptionToOutput 'Mon A' Virtual-1
probe sendCaptionToOutput 'Mon B' Virtual-1
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
tab=$(idOf 'Tab') ma=$(idOf 'Mon A') mb=$(idOf 'Mon B')
for id in "$ma" "$mb"; do probe serverDecorate "$id" >/dev/null; done
sleep .4
probe activateWindowId "$tab" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
lay_out_monitor "$mb" "$tab"
report start
check 'start: the small display is primary' \
    context '[.displayContext.displays[] | select(.name == "Virtual-0" and .primary)] | length == 1'
check 'start: the monitor holds its own cards, sized Connected' \
    context '[.displayContext.displays[] | select(.name == "Virtual-1" and .holdsCards and .sizing == "connected")] | length == 1'
check 'start: the monitor shows its two windows as a layout' monitor_panes 2
check 'start: the small display shows its own card' keeps_own "$tab"

# Spread on the small display leaves the monitor's layout where it is.
spread_key
report spread
check 'spread: Spread opens on the small display' presents cardLine
check 'spread: the monitor keeps showing its two windows' monitor_panes 2
check 'spread: its windows stay monitor panes, held by no card' monitor_pane "$ma"
pick "$tab"
report picked
check 'pick: the monitor still shows its two windows' monitor_panes 2
check 'pick: the small display went back to its own card' keeps_own "$tab"

# Minimized from the dock, it waits in Spread asleep for the monitor.
probe minimizeWindow "$ma" true >/dev/null
sleep 1.2
report minimized
check 'minimize: it waits asleep for the monitor' sleeps_for_monitor "$ma"
check 'minimize: the small display keeps its own card' keeps_own "$tab"

# Brought back from the dock, it is in use on the monitor: the layout that
# fell to one ended into a card (§7), so it is the monitor's Active card.
probe activateWindowId "$ma" >/dev/null
sleep 1.4
report woken
check 'wake: the small display keeps its own card, with no Spread' keeps_own "$tab"
check 'wake: it is the card the monitor shows' shows_on_monitor "$ma"
check 'wake: it has the focus' focused "$ma"
lay_out_monitor "$ma" "$tab"
report laid-out
check 'wake: Meta+B lays the monitor out again' monitor_panes 2

# Clicked on the monitor, in its middle or on its title bar, each window is
# used there.
click_on "$mb"
report clicked-b
check 'click: the small display keeps its own card after a click on the monitor' keeps_own "$tab"
check 'click: the window clicked is a monitor pane, held by no card' monitor_pane "$mb"
check 'click: it has the focus' focused "$mb"
click_title "$ma"
report clicked-a
check 'click again: the small display still keeps its own card' keeps_own "$tab"
check 'click again: the woken window is still a monitor pane' monitor_pane "$ma"
check 'click again: it has the focus' focused "$ma"

# Picked asleep in the monitor's own Spread, it wakes there as the Active
# card, never back in a layout (§7).
probe minimizeWindow "$ma" true >/dev/null
sleep 1.2
report asleep
check 'asleep: it waits asleep for the monitor again' sleeps_for_monitor "$ma"
kad showCardLineOn Virtual-1 >/dev/null
sleep 1.2
report spread-asleep
check 'asleep: the monitor opens its own Spread' presents_on Virtual-1 cardLine
check 'asleep: the small display keeps its card, no Spread' keeps_own "$tab"
pick "$ma"
report picked-asleep
check 'asleep pick: it wakes as the card the monitor shows' shows_on_monitor "$ma"
check 'asleep pick: no layout holds it' monitor_panes 0
check 'asleep pick: the small display went back to its own card' keeps_own "$tab"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 3 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display wake: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: a monitor window woken or clicked there is used there, and Spread stays shut on the small display'
