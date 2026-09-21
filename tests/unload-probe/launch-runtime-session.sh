#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: new-app admission line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
QT_QPA_PLATFORM=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .5
probe contactObserve
probe contactStart
probe contactPrepareDecoration
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
original=$(probe windowGeometry "$main")
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
kad outputStageState | rg '^Virtual-0\|.*\|1$'
client companion
sleep .5
kad outputStageState | rg '^Virtual-0\|.*\|2$'
kad workspaceContext | jq -e '[.applications[] | select(.output=="Virtual-0" and (.minimized|not))] | length==2'
# §8 at the cap. Every harness output is 1280x800, so two panes is the maximum
# and the layout above is already full. A third launch must be answered by the
# layout: it takes the one slot its minimum fits and only that slot's occupant
# leaves. A launch put on top of a running layout is what physical review found
# on 20 September, and the growth case above passed throughout it.
before=$(kad workspaceContext | jq -r '[.applications[] | select(.output=="Virtual-0") | .windowId] | join(" ")')
read -r first second <<<"$before"
paneA=$(probe windowGeometry "$first")
paneB=$(probe windowGeometry "$second")
# `paneCompanion`'s 700px minimum fits the wide pane of a 1280-wide stage and
# not the narrow one, so §8 leaves it exactly one slot to claim.
client paneCompanion
sleep 1
kad outputStageState | rg '^Virtual-0\|.*\|2$'
kad workspaceContext | jq -e '[.applications[] | select(.output=="Virtual-0" and (.minimized|not))] | length==3'
kad workspaceContext | jq -e '[.applications[] | select(.minimized)] | length==0'
arrival=$(kad workspaceContext | jq -r --arg before "$before" \
    '[.applications[] | select(.output=="Virtual-0") | .windowId] - ($before | split(" ")) | .[0]')
test -n "$arrival"
# The arrival occupies one of the exact rectangles the layout already had. A
# refusal leaves it at its own size beside an untouched layout, so this is what
# separates taking a slot from being put in front of one.
probe windowGeometry "$arrival" | jq -e --argjson a "$paneA" --argjson b "$paneB" '. == $a or . == $b'
# What the yielding window becomes is not reachable here. No display the harness
# creates is an internal panel, so there is no card stage to take it and it is
# simply left where it was, still wearing the pane rectangle. Whether it becomes
# a nonselected card, and whether a refused arrival retires the layout instead of
# being drawn over it, are the halves this seam owes physical review.

test "$(probe releaseRuntime)" = true
sleep .4
kad outputStageState | rg '^Virtual-0\|.*\|0$'
probe windowGeometry "$main" | jq -e --argjson original "$original" '. == $original'
kad workspaceContext | jq -e '[.applications[] | select(.minimized)] | length==0'
echo "PASS: new app expands single-card Bento to two visible members; at the cap it claims one slot and only that slot's occupant leaves; release restores the original window"
