#!/usr/bin/env bash
# Spread's chosen card wears a ring, and the ring is part of the card: a
# panel applet's popup standing over the card covers its ring as it covers
# the card, whether the card is alone in Spread or one of two.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: ring popup runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
here=$(dirname "$0")
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .6
probe pointer 4 4
client colouredCompanion "Ring left" 1e6fd9 900 600
sleep .8
client colouredCompanion "Ring right" 2e8b57 900 600
sleep .8
# The band covers the middle of the display, across the chosen card's left
# and right edges, where its ring stands: every pixel of it is the popup's.
covered() {
    local share
    share=$(python3 "$here/capture-band.py" 0 380 1280 40 8a2be2)
    echo "popup share of the band ($1): $share"
    [[ $share == 1.000 ]]
}
kad showCardLine
sleep .8
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"' >/dev/null
client popupBand
sleep .8
covered "two cards"
echo 'PASS: a popup covers the chosen ring of one of two cards'
# One app quits, as from the system tray, with the popup still open: the
# card left is chosen, alone, and Spread shows it.
client closeCompanion "Ring right"
sleep 1
kad workspaceContext | jq -e '[.applications[] | select(.hasCard)] | length == 1' >/dev/null
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"' >/dev/null || { kad showCardLine; sleep .8; }
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"' >/dev/null
covered "lone card"
echo 'PASS: a popup covers the chosen ring of a lone card'
client closePopup
