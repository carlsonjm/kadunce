#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3: the first Card or Bento action on a display adopts
# every eligible window on that display and the current virtual desktop at
# once, and nothing on another display or desktop.
#
# On the tablet, switching Kadunce on and, once every window has been released,
# a first top or side snap each make the carried or used window the Active card
# and every other window there a nonselected card, with no layout begun. A
# first entry let go of before it lands leaves every window Native. On the
# monitor, which cannot own cards, a first side snap organizes every window it
# shows on this desktop into one layout (DECISIONS.md § A display without
# cards organizes everything it shows). A window on another desktop, on either
# display, is left exactly as it was.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: first entry: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
# What KWin says of a window: where it stands, and whether it shows.
state() { probe windowFacts | jq -c --arg c "$1" 'first(.[] | select(.caption == $c)) | {output, x, y, width, height, minimized}'; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, output, hasCard, selected}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.normal) | {caption, output, onCurrentDesktop, minimized, x, y, width, height})')"
}
# The windows on the tablet and this desktop.
tablet='"unload-client", "Entry B", "Entry C"'
cards() { context "[.applications[] | select(.title == ($tablet) and .output == \"Virtual-0\" and .hasCard)] | length == 3"; }
activeIs() { context --arg id "$1" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'; }
oneSelected() { context '[.applications[] | select(.hasCard and .selected)] | length == 1'; }
native() { context '[.applications[] | select(.hasCard)] | length == 0'; }
noLayout() { context '[.displayContext.displays[] | select(.bentoActive)] | length == 0'; }
monitorNative() { context '[.applications[] | select(.title == "Entry Monitor" and .hasCard)] | length == 0'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
# KWin opens a window on the display under the pointer, on the current desktop.
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client titledCompanion 'Entry B' 400 300
client titledCompanion 'Entry C' 400 300
probe pointer 1900 400
client titledCompanion 'Entry Monitor' 400 300
sleep .6
probe sendCaptionToOutput 'Entry Monitor' Virtual-1
vdm createDesktop 1 Two
sleep .3
one=$(vdm org.kde.KWin.VirtualDesktopManager.current)
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
switch() { qdbus6 org.kde.KWin /VirtualDesktopManager org.freedesktop.DBus.Properties.Set org.kde.KWin.VirtualDesktopManager current "$1"; sleep .6; }
switch "$two"
probe pointer 600 400
client titledCompanion 'Entry Elsewhere' 400 300
probe pointer 1900 400
client titledCompanion 'Entry Monitor Elsewhere' 400 300
sleep .6
probe sendCaptionToOutput 'Entry Elsewhere' Virtual-0
probe sendCaptionToOutput 'Entry Monitor Elsewhere' Virtual-1
switch "$one"
probe pointer 600 400
sleep .3
main=$(idOf unload-client)
elsewhere=$(state 'Entry Elsewhere')
monitorElsewhere=$(state 'Entry Monitor Elsewhere')
report setup
check 'the other desktop holds one window on each display' \
    test "$(jq -r .output <<<"$elsewhere") $(jq -r .output <<<"$monitorElsewhere")" = 'Virtual-0 Virtual-1'
untouched() {
    check "$1: the window on the tablet on another desktop is untouched" test "$(state 'Entry Elsewhere')" = "$elsewhere"
    check "$1: the window on the monitor on another desktop is untouched" test "$(state 'Entry Monitor Elsewhere')" = "$monitorElsewhere"
}

# Carries the application's own window from its middle to x y by pointer and
# lets go there. Told to cancel, it carries by finger instead and adds a
# second finger before letting go, which cancels the carry (INPUT.md § Bento).
carry() {
    local id=$1 x=$2 y=$3 cancel=${4:-} px py
    read -r px py < <(probe windowGeometry "$id" | jq -r '"\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
    probe contactFocus >/dev/null
    probe pointer "$px" "$py"
    client armMove
    if [[ $cancel == cancel ]]; then probe down 81 "$px" "$py"; else probe contactButton true; fi
    sleep .15
    if [[ $cancel == cancel ]]; then probe motion 81 "$(((px + x) / 2))" "$(((py + y) / 2))"
    else probe contactMotion "$(((px + x) / 2))" "$(((py + y) / 2))"; fi
    sleep .15
    if [[ $cancel == cancel ]]; then probe motion 81 "$x" "$y"; else probe contactMotion "$x" "$y"; fi
    sleep .3
    echo "carry to $x $y: $(kad nativeCarryState | jq -c '{carrying, destination, destinationPreview}')"
    if [[ $cancel == cancel ]]; then
        probe down 82 900 500
        sleep .15
        echo "second finger: $(kad nativeCarryState | jq -c '{carrying, destination, destinationPreview}')"
        probe up 82
        probe up 81
    else
        probe contactButton false
    fi
    sleep 1
}

probe contactObserve
probe contactStart >/dev/null
probe contactFocus >/dev/null
sleep .3

# Switched on, Kadunce adopts the tablet: the window in use is Active and the
# others on the tablet and this desktop are cards.
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
report switched-on
check 'switched on: every window on the tablet and this desktop is a card' cards
check 'switched on: one of them is Active and the rest wait' oneSelected
check 'switched on: the window on the monitor is not a card' monitorNative
check 'switched on: no layout begins' noLayout
untouched 'switched on'

for edge in top left; do
    test "$(probe releaseRuntime)" = true
    sleep .8
    report "$edge-released"
    check "$edge: released, no window is a card" native

    # Let go of before it lands, the entry changes nothing.
    if [[ $edge == top ]]; then x=640 y=3; else x=3 y=400; fi
    carry "$main" "$x" "$y" cancel
    report "$edge-cancelled"
    check "$edge: a first entry let go of before it lands leaves every window Native" native
    check "$edge: and begins no layout" noLayout

    carry "$main" "$x" "$y"
    report "$edge-entered"
    check "$edge: the carried window is the Active card" activeIs "$main"
    check "$edge: every other window on the tablet and this desktop is a card" cards
    check "$edge: only the carried window is selected" oneSelected
    check "$edge: no layout begins" noLayout
    check "$edge: the window on the monitor is not a card" monitorNative
    untouched "$edge"
done

# The monitor: every window released, the application's window taken there,
# then snapped to the monitor's left edge.
test "$(probe releaseRuntime)" = true
sleep .8
tabletB=$(state 'Entry B')
tabletC=$(state 'Entry C')
probe sendCaptionToOutput unload-client Virtual-1
sleep .8
report monitor-before
check 'monitor: the application window is on the monitor' test "$(state unload-client | jq -r .output)" = Virtual-1
carry "$main" 1283 400
report monitor-entered
monitorId=$(idOf 'Entry Monitor')
twoPanes() { kad outputStageState | grep -q '^Virtual-1|.*|2$'; }
bothShow() { probe windowFacts | jq -e --arg a "$main" --arg b "$monitorId" \
    '[.[] | select((.id == $a or .id == $b) and .output == "Virtual-1" and (.minimized | not))] | length == 2'; }
onLeft() { probe windowGeometry "$main" | jq -e --argjson right "$(probe windowGeometry "$monitorId")" '.x < $right.x'; }
check 'monitor: one layout begins there' \
    context '[.displayContext.displays[] | select(.bentoActive) | .name] == ["Virtual-1"]'
check 'monitor: it holds both windows the monitor shows on this desktop' twoPanes
check 'monitor: both show' bothShow
check 'monitor: the carried window takes the left' onLeft
check 'monitor: the tablet windows are left as they were' test "$(state 'Entry B') $(state 'Entry C')" = "$tabletB $tabletC"
check 'monitor: no window is a card' native
untouched monitor

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .5
if ((failures)); then echo "FAIL: first entry: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a first entry adopts every eligible window on its display and desktop at once, and nothing else'
