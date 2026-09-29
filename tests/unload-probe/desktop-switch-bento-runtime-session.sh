#!/usr/bin/env bash
# The same rule for a Bento layout: a window opened on another desktop never
# joins the first desktop's layout, and never takes the person back there.
set -euo pipefail
trap 'echo "FAIL: desktop switch bento $LINENO" >&2' ERR
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
kad showCardLine
sleep .4
kad showActive
sleep .4
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .8
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
panes=$(probe windowFacts | jq -c '[.[] | select(.class == "unload-client") | {id, x, y, width, height}] | sort_by(.id)')
vdm createDesktop 1 Two
sleep .3
one=$(vdm org.kde.KWin.VirtualDesktopManager.current)
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
switch() { qdbus6 org.kde.KWin /VirtualDesktopManager org.freedesktop.DBus.Properties.Set org.kde.KWin.VirtualDesktopManager current "$1"; sleep .6; }
switch "$two"
client crossCompanion
sleep 1
report two-opened
test "$(vdm org.kde.KWin.VirtualDesktopManager.current)" = "$two"
window 'Cross ownership probe' '.onCurrentDesktop and (.hidden | not) and .width == 400 and .height == 300'
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
python3 - "$(python3 "$(dirname "$0")/capture-band.py" 540 350 200 100 2e8b57)" <<'PY2'
import sys; assert float(sys.argv[1]) > 0.9, sys.argv[1]
PY2
echo 'PASS: a window opened on another desktop never joins the layout or takes the person back'
switch "$one"
report one-back
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
test "$(probe windowFacts | jq -c '[.[] | select(.class == "unload-client" and .caption != "Cross ownership probe") | {id, x, y, width, height}] | sort_by(.id)')" = "$panes"
echo 'PASS: the layout on the first desktop comes back exactly as it was'
