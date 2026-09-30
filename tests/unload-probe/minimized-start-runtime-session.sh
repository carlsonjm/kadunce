#!/usr/bin/env bash
# CARD-LIFECYCLE.md §2, §3 and §7 on the display that can own cards: a window
# minimized before Kadunce is switched on becomes a sleeping card, and picking
# it brings it forward as the Active card, whether picked through Kadunce or as
# the dock and task switcher do.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: minimized start runtime $LINENO" >&2' ERR
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
client titledCompanion "Sleeper" 800 600
sleep .5
sleeper=$(probe windowIdByCaption "Sleeper")
[[ -n $sleeper ]]
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .3
test "$(probe windowMinimized "$sleeper")" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .6
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
kad workspaceContext | jq -e --arg id "$sleeper" '.applications[] | select(.windowId == $id) | .hasCard and .minimized'
kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId != $id'
echo 'PASS: a window minimized before Kadunce switches on is a sleeping card'
# Picked through Kadunce, as the launcher picks an open application.
test "$(kad activateApplicationWindow "$sleeper")" = true
sleep .6
test "$(probe windowMinimized "$sleeper")" = false
kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId == $id and .cardStage.presentation == "active"'
echo 'PASS: picked through Kadunce, it comes forward as the Active card'
# Asleep again, then picked as the dock and task switcher pick a window.
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .6
kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId != $id'
test "$(probe activateWindowId "$sleeper")" = true
sleep .6
test "$(probe windowMinimized "$sleeper")" = false
kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId == $id and .cardStage.presentation == "active"'
echo 'PASS: picked as the dock picks it, it comes forward as the Active card'
