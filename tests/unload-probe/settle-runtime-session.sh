#!/usr/bin/env bash
# CARD-LIFECYCLE.md §5 and §13 on a display that can own cards. A client can
# change its own frame while a layout is being placed --- a terminal reflowing,
# a scale change re-rounding one edge --- and the layout asks for its rects once
# more before it concludes anything from that. A pane that still will not take
# the rect it was given is a window the layout cannot show, so it leaves for
# card ownership and §5 answers what remains. Returning every window to Plasma
# is §13's release and disable answer, and never a settle's.
#
# This needs the tablet fixture: §5's destination only exists on a display that
# can own cards, and on every other display nothing leaves.
set -euo pipefail
trap 'echo "FAIL: settle runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
client crossCompanion
sleep .8
probe pointer 500 350
kad showCardLine
sleep .4
kad showActive
sleep .4
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .8
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
echo 'PASS: the tablet presents a two-pane layout'
# §6 projects the layout into one Spread group, whose panes are no longer held,
# so the client's own resize lands there and the resume places it back. The
# same client then moves itself once more while that placement is settling.
kad showCardLine
sleep .6
client resizeCompanion "Cross ownership" 40
sleep .4
kad showActive
client resizeCompanion "Cross ownership" 40
sleep 1.8
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .bentoActive)] | length == 1'
echo 'PASS: a client that moves itself once while a placement settles keeps its layout'
# A client that will not stay where the layout puts it, however many times it
# is asked, is a window the layout cannot show. §5 sends it to card ownership
# and ends the layout it leaves at one pane; §13 keeps both windows off the
# native desktop until release or disable. Only a placement is observed, so
# this client keeps moving across the whole of one.
kad showCardLine
sleep .6
client resizeCompanion "Cross ownership" 40
sleep .4
kad showActive
for _ in 1 2 3 4 5 6; do
    client resizeCompanion "Cross ownership" 20
    sleep .3
done
sleep 1.2
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|0$'
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .bentoActive)] | length == 0'
kad workspaceContext | jq -e '.cardStage.active'
kad workspaceContext | jq -e '[.applications[] | select(.output == "Virtual-0")] | length == 2'
echo 'PASS: a pane that will not take its rect leaves Bento instead of taking the layout to Plasma'
# §13: release returns both windows, including the one the settle handed on.
test "$(probe releaseRuntime)" = true
sleep .8
kad workspaceContext | jq -e '.cardStage.active == false'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: release returns the windows the settle handed to card ownership'
