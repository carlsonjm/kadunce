#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3 and §12 on the display that can own cards: switched on
# with nothing open, the first window to open becomes the Active card, and once
# the last card closes the next window to open starts the same way.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: start cards runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .3
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
kad workspaceContext | jq -e '.cardStage.active | not'
echo 'PASS: switched on with nothing open, the tablet holds no card'
opens_active() {
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    for attempt in {1..40}; do
        kad workspaceContext | jq -e '.applications | length == 1' >/dev/null && break
        sleep .1
    done
    sleep .5
    local id
    id=$(kad workspaceContext | jq -r '.applications[0].windowId')
    kad workspaceContext | jq -e --arg id "$id" '.cardStage.active
        and .cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
}
opens_active
echo 'PASS: the first window to open becomes the Active card'
kill "$client_pid"
wait "$client_pid" 2>/dev/null || true
sleep .5
kad workspaceContext | jq -e '.cardStage.active | not'
opens_active
echo 'PASS: after the last card closes, the next window to open becomes the Active card'
kill "$client_pid"
wait "$client_pid" 2>/dev/null || true
