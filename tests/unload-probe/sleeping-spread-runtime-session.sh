#!/usr/bin/env bash
# CARD-LIFECYCLE.md §2 on the display that can own cards: a sleeping card
# stands in Spread drawn dimmed, and chosen there, with the keys or a click,
# wakes and is presented as Active, whether it fell asleep while Kadunce ran
# or before it switched on.
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
shots=$(dirname "$XDG_RUNTIME_DIR")
# The mean brightness of the middle of the display, where Spread holds the
# selected card.
middle() {
    python3 "$(dirname "$0")/capture-png.py" 560 340 160 120 "$shots/$1.png"
    python3 -c 'import sys; from PIL import Image; im = Image.open(sys.argv[1]).convert("L"); print(sum(im.getdata()) // (im.width * im.height))' "$shots/$1.png"
}
wake_from_spread() {
    kad showCardLine
    sleep .8
    local awake
    awake=$(middle "awake-$1")
    probe key 106 0
    sleep .6
    kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId == $id and .cardStage.presentation == "cardLine"'
    local asleep
    asleep=$(middle "asleep-$1")
    echo "middle of the selected card: awake $awake, asleep $asleep"
    # Drawn, and dimmed: an empty place would be darker still.
    (( asleep * 10 < awake * 7 && asleep * 10 > awake * 2 ))
    if [[ $1 == click ]]; then
        probe pointer 640 400
        probe button true
        probe button false
    else
        probe key 28 0
    fi
    sleep .8
    test "$(probe windowMinimized "$sleeper")" = false
    kad workspaceContext | jq -e --arg id "$sleeper" '.cardStage.selectedCardId == $id and .cardStage.presentation == "active"'
}
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .6
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
wake_from_spread keys
echo 'PASS: a card asleep since before Kadunce stands dimmed in Spread, and the keys wake it as Active'
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .8
wake_from_spread click
echo 'PASS: a card put to sleep while Kadunce runs stands dimmed in Spread, and a click wakes it as Active'
