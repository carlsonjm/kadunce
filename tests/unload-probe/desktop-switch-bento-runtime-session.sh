#!/usr/bin/env bash
# Each desktop has its own layouts beside its own cards. A window opened on
# another desktop becomes that desktop's card, never a pane of the first
# desktop's layout, and never takes the person back there; the layout comes
# back exactly as it was. A pane KWin's window menu sends to another desktop
# leaves the layout, which ends into card ownership, and becomes that
# desktop's card.
set -Eeuo pipefail
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
evidence=$(dirname "$XDG_RUNTIME_DIR")
cards() { kad workspaceContext | jq -c '[.applications[] | select(.hasCard) | .title] | sort'; }
desktops_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .desktops | join(",")'; }
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
test "$(vdm org.kde.KWin.VirtualDesktopManager.current)" = "$two"
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|0$'
test "$(cards)" = '["Cross ownership probe"]'
python3 - "$(python3 "$(dirname "$0")/capture-band.py" 540 350 200 100 2e8b57)" <<'PY2'
import sys; assert float(sys.argv[1]) > 0.9, sys.argv[1]
PY2
echo "PASS: a window opened on another desktop becomes that desktop's card, never a pane, and never takes the person back"
switch "$one"
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
test "$(probe windowFacts | jq -c '[.[] | select(.class == "unload-client" and .caption != "Cross ownership probe") | {id, x, y, width, height}] | sort_by(.id)')" = "$panes"
echo 'PASS: the layout on the first desktop comes back exactly as it was'
probe sendToDesktop 'Ordinary neighbor probe' "$two"
sleep .8
test "$(desktops_of 'Ordinary neighbor probe')" = "$two"
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|0$'
test "$(cards)" = '["unload-client"]'
switch "$two"
test "$(cards)" = '["Cross ownership probe","Ordinary neighbor probe"]'
test -z "$(kad ownershipViolations)"
echo "PASS: a pane the window menu sends to another desktop ends the layout and becomes that desktop's card"
