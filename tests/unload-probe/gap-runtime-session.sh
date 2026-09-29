#!/usr/bin/env bash
# The gutter around a card is quiet. An application drawing its own title bar
# keeps an invisible resize border outside its frame, wider than the gutter a
# card keeps, and over it the pointer takes a resize shape; Breeze with no
# borders keeps one the same way, and stands in for it here. With the window
# a card, the pointer in the gutter belongs to no window, and a press and drag
# there moves and resizes nothing; back over the card the window has it
# again, and switching Kadunce off leaves no hold.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
set -Eeuo pipefail
trap 'echo "FAIL: gap runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
card() { probe windowFacts | jq -r --arg c "$caption" "first(.[] | select(.caption == \$c)) | $1"; }
# Two motions, so a pointer focus that follows the first is settled.
point() { probe pointer "$1" "$2"; sleep .15; probe pointer "$1" $(( $2 + 1 )); sleep .15; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .3
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
for attempt in {1..40}; do
    kad workspaceContext | jq -e '.cardStage.presentation == "active"' >/dev/null && break
    sleep .1
done
sleep .8
caption=$(probe windowFacts | jq -r 'first(.[] | select(.normal)) | .caption')
kad workspaceContext | jq -e '.cardStage.presentation == "active"' >/dev/null
left=$(card '.x | floor')
middle=$(( $(card '.y | floor') + $(card '.height | floor') / 2 ))
gap=$(( left - 3 ))
test "$gap" -ge 1
# The window's border reaches into the gutter; this is what took the pointer.
test "$(probe inMargin "$caption" "$gap" "$middle")" = true
point $(( left + 60 )) "$middle"
test "$(probe pointerFocus)" = "$caption"
echo "PASS: the card keeps a resize border in the gutter, and over the card its window has the pointer"

point "$gap" "$middle"
test "$(probe mouseIntercepted)" = true
test -z "$(probe pointerFocus)"
before=$(card '[.x, .y, .width, .height] | map(floor) | join(",")')
probe button true
sleep .1
probe pointer 1 "$middle"
sleep .1
probe pointer 1 $(( middle + 60 ))
sleep .1
probe button false
sleep .5
test "$(card '[.x, .y, .width, .height] | map(floor) | join(",")')" = "$before"
echo "PASS: in the gutter the pointer is no window's, and a press and drag there moves and resizes nothing"

point $(( left + 60 )) "$middle"
test "$(probe mouseIntercepted)" = false
test "$(probe pointerFocus)" = "$caption"
echo "PASS: back over the card, its window has the pointer again"

point "$gap" "$middle"
test "$(probe mouseIntercepted)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .5
test "$(probe mouseIntercepted)" = false
echo "PASS: switching Kadunce off leaves no hold on the pointer"
