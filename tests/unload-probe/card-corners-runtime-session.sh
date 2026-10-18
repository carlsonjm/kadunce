#!/usr/bin/env bash
# The Active card's corners are trimmed round, and what shows in them is what
# lies under the card: the desktop background, even where the cards it hides
# stand in the same place. Those cards are drawn nowhere, so they must hide
# nothing under them either. A pane of a layout, with a card waiting behind
# it, shows the background in its corners the same way.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -Eeuo pipefail
trap 'echo "FAIL: card corners $LINENO" >&2' ERR
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
client desktopSurface
client colouredCompanion Back c03a3a 700 500
client colouredCompanion Middle 2980b9 700 500
client colouredCompanion Front 30a050 700 500
sleep .8
back=$(probe windowIdByCaption Back)
middle=$(probe windowIdByCaption Middle)
front=$(probe windowIdByCaption Front)
[[ -n $back && -n $middle && -n $front ]]
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
context() { kad workspaceContext | jq -e "$@" >/dev/null; }
failures=0
check() {
    local name=$1; shift
    if "$@"; then echo "ok: $name"; else echo "FAIL: $name" >&2; failures=$((failures + 1)); fi
}
# Each card is Active in turn, so each stands in the Active place, one behind
# the next.
for id in "$back" "$middle" "$front"; do
    probe activateWindowId "$id" >/dev/null
    sleep .8
done
# The four corners of a window, each a square of 2 by 2, and its middle; how
# much of each is the background, and how much is each card's colour.
look() {
    local x y w h
    read -r x y w h <<<"$(probe windowGeometry "$1" | jq -r '[.x, .y, .width, .height] | map(floor) | @tsv')"
    # The pointer, out of the way of every area looked at.
    probe pointer "$((x + w / 2 + 150))" "$((y + h / 2))" >/dev/null
    sleep .5
    python3 "$(dirname "$0")/capture-shares.py" \
        topLeft="$x,$y,2,2" topRight="$((x + w - 2)),$y,2,2" \
        bottomLeft="$x,$((y + h - 2)),2,2" bottomRight="$((x + w - 2)),$((y + h - 2)),2,2" \
        middle="$((x + w / 2 - 10)),$((y + h / 2 - 10)),20,20" -- 334455 c03a3a 2980b9 30a050
}
rounded() {
    local view
    view=$(look "$1")
    echo "corners of $2: $view"
    jq -e --arg c "$3" '.middle[$c] > 0.9
        and ([.topLeft, .topRight, .bottomLeft, .bottomRight] | all(.["334455"] > 0.7))' <<<"$view" >/dev/null
}
printf 'facts %s\n' "$(probe windowFacts | jq -c '[.[] | select(.caption == "Back" or .caption == "Middle" or .caption == "Front") | {caption, x, y, width, height, hidden}]')"
check 'active: the front card is the Active card' \
    context --arg id "$front" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
check 'active: its corners show the background, not the cards behind it' rounded "$front" Front 30a050
check 'active: Kadunce trims them' \
    context --arg id "$front" 'first(.applications[] | select(.windowId == $id)) | .corners | .rounded or (.available | not)'
# A layout of two, a card waiting behind them: each pane's corners show the
# background too.
probe key 48 125 >/dev/null
sleep 1.4
printf 'layout %s\n' "$(kad workspaceContext | jq -c '[.applications[] | {title, pane, hasCard}]')"
check 'layout: the front card is a pane' context --arg id "$front" 'first(.applications[] | select(.windowId == $id)) | .pane'
check 'layout: its corners show the background' rounded "$front" Front 30a050
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: card corners: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: an Active card and a pane show the background in their corners, over the cards they hide'
