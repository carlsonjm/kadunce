#!/usr/bin/env bash
# CARD-LIFECYCLE.md §2 on the display that can own cards: a sleeping card
# chosen in Spread with the keys wakes and is presented as Active, whether it
# fell asleep while Kadunce ran or before it switched on.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: sleeping spread runtime $LINENO" >&2' ERR
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
client titledCompanion "Sleeper" 900 600
sleep .8
sleeper=$(probe windowIdByCaption "Sleeper")
[[ -n $sleeper ]]
wake_from_spread() {
    kad showCardLine
    sleep .8
    probe key 106 0
    sleep .6
    kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId == $id and .cardStage.presentation == "cardLine"'
    probe key 28 0
    sleep .8
    test "$(probe windowMinimized "$sleeper")" = false
    kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId == $id and .cardStage.presentation == "active"'
}
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .6
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
wake_from_spread
echo 'PASS: a card asleep since before Kadunce, chosen in Spread, wakes as Active'
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .8
wake_from_spread
echo 'PASS: a card put to sleep while Kadunce runs, chosen in Spread, wakes as Active'
