#!/usr/bin/env bash
# A monitor unplugged and plugged back in. KWin moves windows between displays
# when that happens: it evacuates the display that went away, and on replug it
# puts windows back where they last stood on that layout, cards included.
# CARD-LIFECYCLE.md §3: a window arriving on the display that holds cards
# arrives as a card. §14: only release or disable returns a card to the
# desktop. So after every change, a window on the tablet is a card and a card's
# window is on the tablet, and a window that arrived can be seen and picked.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: output unplug: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
facts() { probe windowFacts | jq -e --arg c "$1" "first(.[] | select(.caption == \$c)) | $2"; }
idOf() { kad workspaceContext | jq -r --arg c "$1" 'first(.applications[] | select(.title == $c)) | .windowId'; }
green() { python3 "$(dirname "$0")/capture-band.py" 0 0 1280 800 2e8b57; }
atLeast() { python3 -c 'import sys; sys.exit(0 if float(sys.argv[1]) >= float(sys.argv[2]) else 1)' "$1" "$2"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, output, hasCard, selected}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height})')"
}
# The contract both halves share: the card and its window name one display.
whole() {
    context '[.applications[] | select(.hasCard and .output != "Virtual-0")] | length == 0' \
        && context '[.applications[] | select(.output == "Virtual-0" and (.hasCard | not) and (.minimized | not))] | length == 0'
}
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
# Two windows the person left on the monitor, one of them maximized there.
client crossCompanion
client ordinaryCompanion
sleep .8
probe sendCaptionToOutput 'Cross ownership probe' Virtual-1
probe sendCaptionToOutput 'Ordinary neighbor probe' Virtual-1
sleep .3
probe maximizeCaption 'Ordinary neighbor probe'
sleep .3
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
report start
main=$(idOf unload-client)
check 'the tablet presents its one card; the monitor windows are plain' \
    context --arg m "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $m
        and ([.applications[] | select(.hasCard)] | length == 1)'

unplug
report unplugged
check 'unplugged: KWin moved both monitor windows onto the tablet' \
    facts 'Cross ownership probe' '.output == "Virtual-0"'
check 'unplugged: every window on the tablet is a card, every card is on the tablet' whole
check 'unplugged: the card in front is still the one the person was using' \
    context --arg m "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $m'
check 'unplugged: an arrival waits hidden behind the Active card' test "$(green)" = 0.000
kad showCardLine
sleep .8
check 'unplugged: the arrival is drawn in Spread on the tablet' atLeast "$(green)" 0.01
kad showActive
sleep .6
# The person picks the maximized arrival, as the dock does.
probe activateWindowId "$(idOf 'Ordinary neighbor probe')"
sleep .8
report picked
check 'unplugged: a picked arrival becomes the Active card on the tablet' \
    context --arg id "$(idOf 'Ordinary neighbor probe')" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'

replug
report replugged
check 'replugged: every card is still owned and on the tablet' whole
check 'replugged: KWin did not keep a card on the monitor' \
    facts 'Cross ownership probe' '.output == "Virtual-0"'
check 'replugged: the Active card is still Active, not released by being re-maximized' \
    context --arg id "$(idOf 'Ordinary neighbor probe')" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
check 'replugged: the Active card stands inside the tablet' \
    facts 'Ordinary neighbor probe' '.output == "Virtual-0" and .x >= 0 and .x + .width <= 1280 and .y >= 0 and .y + .height <= 800'
kad showCardLine
sleep .8
check 'replugged: the returned card is drawn in Spread on the tablet' atLeast "$(green)" 0.01
kad showActive
sleep .6

# The sequence from the hand test: Kadunce switched off and on while the
# monitor is out, which adopts the arrivals, then the monitor comes back.
unplug
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
report reenabled
check 'switched on while unplugged: the tablet holds every window as a card' \
    context '[.applications[] | select(.hasCard)] | length == 3'
replug
report replugged-again
check 'replugged after switching on: every card is on the tablet' whole

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\")) | length == 3 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
if (( failures > 0 )); then
    echo "FAIL: output unplug: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: a monitor unplugged and plugged back in leaves every card and its window on the tablet'
