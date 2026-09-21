#!/usr/bin/env bash
# CARD-LIFECYCLE.md §7 and §5 on a display that can own cards. Minimizing a
# pane hands it to card ownership asleep; the layout keeps nothing it does not
# show; and the layout that falls to one visible pane ends into a card. Waking
# the sleeping window never puts it back in Bento.
#
# This needs the tablet fixture, because §5's destination only exists on a
# display that can own cards. On every other display nothing can hold the
# window, and §5 keeps it where it is instead.
set -euo pipefail
trap 'echo "FAIL: sleeping pane $LINENO" >&2' ERR
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
client companion
sleep .8
# §3's Bento action names a pair on a display that can own cards, so the two
# tablet windows become the two panes and no card is left beside them.
probe pointer 500 350
kad showCardLine
sleep .4
kad showActive
sleep .4
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .8
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
panes=$(kad workspaceContext | jq -r '[.applications[] | select(.output == "Virtual-0") | .windowId] | .[]')
sleeper=$(head -1 <<<"$panes")
survivor=$(tail -1 <<<"$panes")
test "$sleeper" != "$survivor"
echo 'PASS: the tablet presents a two-pane layout and no card beside it'
# §7: the minimized pane leaves Bento at once. §5 then has one visible pane
# left, so the layout ends and that pane becomes a card too. Neither window
# goes back to Plasma: §13 keeps that for release and disable.
probe minimizeWindow "$sleeper" true
sleep 1
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|0$'
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .bentoActive)] | length == 0'
test "$(probe windowMinimized "$sleeper")" = true
test "$(probe windowMinimized "$survivor")" = false
kad workspaceContext | jq -e '.cardStage.active'
echo 'PASS: a minimized pane leaves Bento asleep and the last visible pane ends the layout into a card'
# §7: a minimized card never silently returns to Bento. Waking it is an
# ordinary card waking, so the display must still have no layout afterwards.
probe minimizeWindow "$sleeper" false
sleep 1
test "$(probe windowMinimized "$sleeper")" = false
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|0$'
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .bentoActive)] | length == 0'
echo 'PASS: waking the sleeping card does not put it back into Bento'
# §13: only release returns a managed window to Plasma, and it returns both.
test "$(probe releaseRuntime)" = true
sleep .8
test "$(probe windowMinimized "$sleeper")" = false
test "$(probe windowMinimized "$survivor")" = false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: release returns both windows and unload leaves nothing held'
