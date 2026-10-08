#!/usr/bin/env bash
# CARD-LIFECYCLE.md §4 on the display that can own cards: a window hidden from
# the task switcher is never held as a card. Opening after, it meets the same
# eligibility test as it does here.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: switcher hidden runtime $LINENO" >&2' ERR
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
client titledCompanion "Hidden" 800 600
sleep .5
test "$(probe setSkipSwitcher "Hidden" true)" = true
hidden=$(probe windowIdByCaption "Hidden")
[[ -n $hidden ]]
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .6
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
kad workspaceContext | jq -e '.cardStage.active'
kad workspaceContext | jq -e --arg id "$hidden" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 0'
echo 'PASS: a window hidden from the task switcher when Kadunce switches on is not a card'
