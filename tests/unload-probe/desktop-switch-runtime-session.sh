#!/usr/bin/env bash
# Every virtual desktop has its own cards (CARD-LIFECYCLE.md). A desktop shown
# for the first time takes a window opened there as its Active card, its Spread
# holds only its own cards, and each desktop comes back as it was left, with a
# Spread left open closed into Active. A card closed on a desktop not shown
# leaves that desktop's other cards alone. A card KWin's window menu sends to
# another desktop leaves at its own place and becomes that desktop's card; one
# put on every desktop is no desktop's card; a removed desktop's cards become
# the cards of the desktop KWin moves them to; and switching Kadunce off
# returns every desktop's windows.
set -Eeuo pipefail
trap 'echo "FAIL: desktop switch $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
evidence=$(dirname "$XDG_RUNTIME_DIR")
places() { probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, x, y, width, height}) | sort_by(.caption)'; }
place() { jq -c --arg c "$2" 'first(.[] | select(.caption == $c)) | {x, y, width, height}' <<<"$1"; }
cards() { kad workspaceContext | jq -c '[.applications[] | select(.hasCard) | .title] | sort'; }
context() { kad workspaceContext | jq -e "$1" >/dev/null; }
desktops_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .desktops | join(",")'; }
client ordinaryCompanion
client titledCompanion 'Third card probe' 500 360
sleep .8
before=$(places)
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
test "$(cards)" = '["Ordinary neighbor probe","Third card probe","unload-client"]'
context '.cardStage.presentation == "active"'
vdm createDesktop 1 Two
sleep .3
one=$(vdm org.kde.KWin.VirtualDesktopManager.current)
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
# A desktop something else makes while Kadunce runs waits to be entered,
# however much dissolving happens meanwhile: a script that makes one and then
# sends a window to it must find it there.
vdm createDesktop 2 Studio
sleep .3
studio=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v -e "$one" -e "$two" | head -1)
test -n "$studio"
switch() { qdbus6 org.kde.KWin /VirtualDesktopManager org.freedesktop.DBus.Properties.Set org.kde.KWin.VirtualDesktopManager current "$1"; sleep .6; }
# A Spread left open on the first desktop waits there, closed into Active.
kad showCardLine
sleep .5
context '.cardStage.presentation == "cardLine"'
switch "$two"
test "$(vdm org.kde.KWin.VirtualDesktopManager.current)" = "$two"
context '.cardStage.active | not'
client crossCompanion
sleep 1
test "$(cards)" = '["Cross ownership probe"]'
context '.cardStage.presentation == "active"'
# It is drawn: its colour fills the middle of the tablet.
python3 - "$(python3 "$(dirname "$0")/capture-band.py" 540 350 200 100 2e8b57)" <<'PY2'
import sys; assert float(sys.argv[1]) > 0.9, sys.argv[1]
PY2
client titledCompanion 'Second desktop probe' 500 360
sleep 1
test "$(cards)" = '["Cross ownership probe","Second desktop probe"]'
echo 'PASS: a desktop shown for the first time takes its windows as its own cards'
own=$(kad workspaceContext | jq -c '[.applications[] | select(.hasCard) | .windowId]')
# The bottom swipe itself is desktop-bezel-runtime's; paging here is by touch.
kad showCardLine
sleep .5
context '.cardStage.presentation == "cardLine"'
for pass in 1 2; do
    probe down 72 300 400
    for x in 400 500 600 700 800 900; do probe motion 72 "$x" 400; sleep .03; done
    probe up 72
    sleep .8
    jq -e --arg sel "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" 'index($sel) != null' <<<"$own" >/dev/null
done
echo 'PASS: Spread on the second desktop holds only its own cards'
client closeCompanion 'Third card probe'
sleep .6
switch "$one"
test "$(cards)" = '["Ordinary neighbor probe","unload-client"]'
context '.cardStage.presentation == "active"'
echo 'PASS: the first desktop comes back as it was left, less the card closed while it was not shown'
qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -q "$studio"
vdm removeDesktop "$studio"
sleep .5
echo 'PASS: a desktop made elsewhere and never entered is not dissolved'
probe sendToDesktop 'Ordinary neighbor probe' "$two"
sleep .8
test "$(desktops_of 'Ordinary neighbor probe')" = "$two"
test "$(cards)" = '["unload-client"]'
test "$(place "$(places)" 'Ordinary neighbor probe')" = "$(place "$before" 'Ordinary neighbor probe')"
switch "$two"
test "$(cards)" = '["Cross ownership probe","Ordinary neighbor probe","Second desktop probe"]'
echo "PASS: a card the window menu sends to another desktop leaves at its own place and becomes that desktop's card"
probe setOnAllDesktops 'Second desktop probe' true
sleep .6
test "$(cards)" = '["Cross ownership probe","Ordinary neighbor probe"]'
probe windowFacts | jq -e 'first(.[] | select(.caption == "Second desktop probe")) | (.hidden | not) and .width == 500 and .height == 360' >/dev/null
switch "$one"
test "$(cards)" = '["unload-client"]'
echo 'PASS: a window put on every desktop leaves the cards at its own place and stays plain on each'
vdm removeDesktop "$two"
sleep .8
test "$(vdm org.kde.KWin.VirtualDesktopManager.count)" = 1
test "$(cards)" = '["Cross ownership probe","Ordinary neighbor probe","unload-client"]'
echo "PASS: a removed desktop's cards become the cards of the desktop KWin moves them to"
test -z "$(kad ownershipViolations)"
echo 'PASS: no window had two owners'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
after=$(places)
test "$(place "$after" 'unload-client')" = "$(place "$before" 'unload-client')"
test "$(place "$after" 'Ordinary neighbor probe')" = "$(place "$before" 'Ordinary neighbor probe')"
jq -e 'first(.[] | select(.caption == "Cross ownership probe")) | .width == 400 and .height == 300' <<<"$after" >/dev/null
jq -e 'first(.[] | select(.caption == "Second desktop probe")) | .width == 500 and .height == 360' <<<"$after" >/dev/null
probe windowFacts | jq -e 'map(select(.class == "unload-client") | .hidden | not) | all' >/dev/null
echo "PASS: switching Kadunce off returns every desktop's windows"
