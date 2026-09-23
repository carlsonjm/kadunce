#!/usr/bin/env bash
# Until each virtual desktop has its own cards (Table), cards live on the
# desktop where ownership started and every other desktop is plain Plasma: a
# window opened there is not a card, Spread does not open there, and the first
# desktop's cards come back exactly as they were, never a Spread left open.
set -euo pipefail
trap 'echo "FAIL: desktop switch $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
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
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .5
client ordinaryCompanion
sleep .8
shots=$(dirname "$XDG_RUNTIME_DIR")
report() {
    python3 "$(dirname "$0")/capture-png.py" 0 0 2560 800 "$shots/$1.png"
    printf '%s cards %s\n' "$1" "$(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId, apps: [.applications[] | {title, hasCard, focused}]}')"
    printf '%s facts %s\n' "$1" "$(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, onCurrentDesktop, hidden, output, x, y, width, height})')"
}
context() { kad workspaceContext | jq -e "$1" >/dev/null; }
window() { probe windowFacts | jq -e --arg c "$1" "first(.[] | select(.caption == \$c)) | $2" >/dev/null; }
report one
first=$(kad workspaceContext | jq -c '[.applications[] | .windowId] | sort')
test "$(jq length <<<"$first")" = 2
context '.cardStage.presentation == "active" and ([.applications[] | select(.hasCard)] | length == 2)'
vdm createDesktop 1 Two
sleep .3
one=$(vdm org.kde.KWin.VirtualDesktopManager.current)
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
switch() { qdbus6 org.kde.KWin /VirtualDesktopManager org.freedesktop.DBus.Properties.Set org.kde.KWin.VirtualDesktopManager current "$1"; sleep .6; }
# Spread left open on the first desktop does not wait there.
kad showCardLine
sleep .5
context '.cardStage.presentation == "cardLine"'
switch "$two"
test "$(vdm org.kde.KWin.VirtualDesktopManager.current)" = "$two"
client crossCompanion
sleep 1
report two-opened
test "$(vdm org.kde.KWin.VirtualDesktopManager.current)" = "$two"
window 'Cross ownership probe' '.onCurrentDesktop and (.hidden | not) and .width == 400 and .height == 300'
context '[.applications[] | select(.hasCard)] | length == 0'
# It is drawn: its colour fills the middle of the tablet.
python3 - "$(python3 "$(dirname "$0")/capture-band.py" 540 350 200 100 2e8b57)" <<'PY2'
import sys; assert float(sys.argv[1]) > 0.9, sys.argv[1]
PY2
echo 'PASS: a window opened on another desktop stays a plain window there'
kad showCardLine
sleep .5
probe down 71 640 790
for y in 740 680 600 520; do probe motion 71 640 "$y"; sleep .03; done
probe up 71
sleep .6
test "$(vdm org.kde.KWin.VirtualDesktopManager.current)" = "$two"
python3 - "$(python3 "$(dirname "$0")/capture-band.py" 540 350 200 100 2e8b57)" <<'PY2'
import sys; assert float(sys.argv[1]) > 0.9, sys.argv[1]
PY2
probe windowAt 640 400 | jq -e '.target.caption == "Cross ownership probe"' >/dev/null
echo 'PASS: Spread does not open on another desktop'
switch "$one"
report one-back
context '.cardStage.presentation == "active"'
test "$(kad workspaceContext | jq -c '[.applications[] | select(.hasCard) | .windowId] | sort')" = "$first"
context '[.applications[] | select(.title == "Cross ownership probe")] | length == 0'
kad showCardLine
sleep .5
probe down 72 300 400
for x in 400 500 600 700 800 900; do probe motion 72 "$x" 400; sleep .03; done
probe up 72
sleep .8
report one-paged
jq -e --arg sel "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" 'index($sel) != null' <<<"$first" >/dev/null
echo 'PASS: the first desktop keeps its cards, and Spread there pages only through them'
