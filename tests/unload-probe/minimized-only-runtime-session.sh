#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3 on the display that can own cards: switched on while
# every window there is minimized, Kadunce holds nothing and wakes nothing;
# the first window then picked, as the dock picks it, starts the cards as the
# Active card, as a window opening there would.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: minimized only runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
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
main=$(probe windowIdByCaption "unload-client")
sleeper=$(probe windowIdByCaption "Sleeper")
[[ -n $main && -n $sleeper ]]
test "$(probe minimizeWindow "$main" true)" = true
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .6
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
kad workspaceContext | jq -e '.cardStage.active | not'
test "$(probe windowMinimized "$main")" = true
test "$(probe windowMinimized "$sleeper")" = true
echo 'PASS: switched on with every window minimized, Kadunce holds nothing and wakes nothing'
test "$(probe activateWindowId "$sleeper")" = true
sleep 1
test "$(probe windowMinimized "$sleeper")" = false
kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.active and .cardStage.presentation == "active"
    and .cardStage.selectedCardId == $id'
test "$(probe windowMinimized "$main")" = true
kad workspaceContext | jq -e --arg id "$main" '.applications[] | select(.windowId == $id) | .hasCard and .minimized'
echo 'PASS: the window then picked starts the cards as the Active card; the other sleeps as a card'
