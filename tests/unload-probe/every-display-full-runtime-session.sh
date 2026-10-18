#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, with the monitor's layout full:
# Virtual-0 at scale 2 is small, sized Solo and primary, and
# Virtual-1 is large and sized Connected. Its three windows are too big for
# more than two to stand side by side, so two make its layout and the third
# waits in its own Spread. Picked there, the waiting card takes the place of
# the pane used longest ago, which waits in its turn; the pane used last
# stays, and the small display keeps its own card. Done twice, with the other
# pane used longest ago the second time. The small display's card carried by
# its title bar onto the full layout takes the place of the pane used longest
# ago, and carried back it leaves that pane its place again. Then a pane
# carried by its title bar to the monitor's top edge is the card it shows
# alone. Every check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: every display full: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, output, hasCard, pane, stage, paneStack, stackId, stackSize, focused, minimized}],
        stages: [.displayContext.displays[] | {name, shows, s: .cardStage.presentation, a: .cardStage.activeCardId}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden, minimized})')"
    echo "state stages-$1 $(kad outputStageState | tr '\n' ' ')"
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
shows_on_monitor() { context --arg id "$1" 'first(.displayContext.displays[] | select(.name == "Virtual-1")) | .cardStage | .presentation == "active" and .activeCardId == $id'; }
keeps_own() { context --arg id "$1" 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .cardStage | .presentation == "active" and .activeCardId == $id'; }
# The monitor's own Spread, opened as its dock does, without using a window.
monitor_spread() { kad showCardLineOn Virtual-1 >/dev/null; sleep 1.2; }
monitor_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .pane and .output == "Virtual-1" and (.hasCard | not) and (.minimized | not)'; }
monitor_panes() { context --argjson n "$1" '[.applications[] | select(.pane and .output == "Virtual-1")] | length == $n'; }
waits_for_monitor() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id))
    | .hasCard and .stage == "Virtual-1" and (.pane | not) and (.minimized | not)'; }
panes_of_monitor() { kad workspaceContext | jq -r '[.applications[] | select(.pane and .output == "Virtual-1") | .windowId] | .[]'; }
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
    echo "state carry-$id $(kad nativeCarryState | jq -c '{carrying, lineCarrying, destinationRect}')"
    probe contactButton false >/dev/null
    sleep 1.6
}
# Carried by its title bar and let go at once at a point, sooner than a rest
# on a window that stacks.
carry_to() {
    local id=$1 tx=$2 ty=$3 x y
    read -r x y <<<"$(title_at "$id")"
    [[ -n ${y:-} ]] || { echo "no title bar on $id"; return 0; }
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe contactButton true >/dev/null
    sleep .15
    probe contactMotion "$((x + 40))" "$((y + 20))" >/dev/null
    sleep .1
    probe contactMotion "$(((x + tx) / 2))" "$(((y + ty) / 2))" >/dev/null
    sleep .1
    probe contactMotion "$tx" "$ty" >/dev/null
    sleep .2
    echo "state carry-$id $(kad nativeCarryState | jq -c '{carrying, destination, destinationRect, stackOnto}')"
    probe contactButton false >/dev/null
    sleep 1.6
}
middle_of() { probe windowGeometry "$1" | jq -r '[.x + .width / 2, .y + .height / 2] | map(floor) | @tsv'; }
fills_monitor() { probe windowFacts | jq -e --arg id "$1" --argjson w "$2" 'first(.[] | select(.id == $id)) | .width >= $w * 0.9'; }
waiting_for_monitor() { kad workspaceContext | jq -r 'first(.applications[] | select(.hasCard and .stage == "Virtual-1" and (.pane | not) and (.minimized | not))) | .windowId'; }
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
client titledCompanion 'Tab' 500 400
sleep .5
# Two of these fit side by side on the large display, never three.
for title in 'Mon A' 'Mon B' 'Mon C'; do client titledCompanion "$title" 1200 900; sleep .5; done
probe sendCaptionToOutput 'Tab' Virtual-0
for title in 'Mon A' 'Mon B' 'Mon C'; do probe sendCaptionToOutput "$title" Virtual-1; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
for title in 'Mon A' 'Mon B' 'Mon C'; do probe serverDecorate "$(idOf "$title")" >/dev/null; done
sleep .4
tab=$(idOf 'Tab')
probe activateWindowId "$tab" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
lay_out_monitor "$(idOf 'Mon C')" "$tab"
report start
check 'start: the monitor shows a layout of two' monitor_panes 2
mapfile -t panes < <(panes_of_monitor)
first=${panes[0]:-} second=${panes[1]:-}
waiting=$(waiting_for_monitor)
check 'start: the third waits in Spread for the monitor' waits_for_monitor "$waiting"
check 'start: the small display shows its own card' keeps_own "$tab"

# The second pane is used longest ago, so it gives its place to the pick.
probe activateWindowId "$second" >/dev/null
sleep .6
probe activateWindowId "$first" >/dev/null
sleep .6
probe activateWindowId "$tab" >/dev/null
sleep .6
monitor_spread
pick "$waiting"
report picked
check 'pick: the card picked is a monitor pane' monitor_pane "$waiting"
check 'pick: the pane used last stays' monitor_pane "$first"
check 'pick: the pane used longest ago waits in Spread for the monitor' waits_for_monitor "$second"
check 'pick: the monitor shows two' monitor_panes 2
check 'pick: the small display went back to its own card' keeps_own "$tab"

# Now the first pane is used longest ago.
probe activateWindowId "$first" >/dev/null
sleep .6
probe activateWindowId "$waiting" >/dev/null
sleep .6
probe activateWindowId "$tab" >/dev/null
sleep .6
monitor_spread
pick "$second"
report picked-again
check 'again: the card picked is a monitor pane' monitor_pane "$second"
check 'again: the pane used last stays' monitor_pane "$waiting"
check 'again: the pane used longest ago waits in Spread for the monitor' waits_for_monitor "$first"
check 'again: the monitor shows two' monitor_panes 2
check 'again: the small display went back to its own card' keeps_own "$tab"

# The small display's card, carried by its title bar onto the full layout,
# takes the place of the pane used longest ago, which waits behind the layout.
probe activateWindowId "$second" >/dev/null
sleep .6
probe activateWindowId "$waiting" >/dev/null
sleep .6
probe activateWindowId "$tab" >/dev/null
sleep .6
read -r mx my <<<"$(middle_of "$waiting")"
carry_to "$tab" "${mx:-0}" "${my:-0}"
report carried-in
check 'carried in: the card carried is a monitor pane' monitor_pane "$tab"
check 'carried in: the pane used last stays' monitor_pane "$waiting"
check 'carried in: the pane used longest ago waits in Spread for the monitor' waits_for_monitor "$second"
check 'carried in: the monitor shows two' monitor_panes 2

# Carried back, it leaves the pane it pushed out its place again.
read -r sx sy <<<"$(kad workspaceContext | jq -r 'first(.displayContext.displays[] | select(.name == "Virtual-0")) | .geometry | [.x + .width / 2, .y + .height / 2] | map(floor) | @tsv')"
carry_to "$tab" "${sx:-0}" "${sy:-0}"
report carried-back
check 'carried back: the small display shows it as its own card' keeps_own "$tab"
check 'carried back: the pane it pushed out is a monitor pane again' monitor_pane "$second"
check 'carried back: the other pane stays' monitor_pane "$waiting"
check 'carried back: the monitor shows two' monitor_panes 2
check 'carried back: the card waiting before still waits in Spread for the monitor' waits_for_monitor "$first"
check 'carried back: the pane that came back stands above the cards hidden behind the layout' \
    jq -e --arg back "$second" --arg hidden "$first" 'map(.id) | index($back) > index($hidden)' < <(probe windowFacts)

# Carried by its title bar to the monitor's top edge, a pane is the Active
# card the monitor shows alone, and the pane beside it waits in Spread for it.
read -r gy gw <<<"$(kad workspaceContext | jq -r 'first(.displayContext.displays[] | select(.name == "Virtual-1")) | .geometry | [.y, .width] | map(floor) | @tsv')"
read -r tx _ <<<"$(title_at "$second")"
title_drag "$second" "$((tx + 30))" "$((gy + 200))" "$tx" "$((gy + 100))" "$tx" "$((gy + 40))" "$tx" "$((gy + 12))" "$tx" "$((gy + 2))"
report top
check 'top: the pane carried there is the Active card the monitor shows' shows_on_monitor "$second"
check 'top: it is alone there, with no layout' monitor_panes 0
check 'top: it fills the monitor' fills_monitor "$second" "${gw:-0}"
check 'top: the pane beside it waits in Spread for the monitor' waits_for_monitor "$waiting"
check 'top: the small display keeps its own card' keeps_own "$tab"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 4 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display full: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: a full monitor layout gives the pane used longest ago to a card picked for it'
