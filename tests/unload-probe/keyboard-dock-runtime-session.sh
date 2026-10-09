#!/usr/bin/env bash
# A dock that gives its room up to the keys lends it to them, not to a card.
# A probe dock along the tablet's bottom edge reserves its height, and gives
# the reservation up while the keys are on screen and takes it back once they
# have gone, as a bottom surface does. An Active card chosen while the keys are
# up stops at the dock's edge, as does the one they typed into once they go;
# and a card sized before the dock took its room is sized again when it does.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
set -euo pipefail
trap 'echo "FAIL: keyboard dock runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
state() { probe keyboardState; }
frame() { probe frames | jq -c --arg t "$1" '.[$t]'; }
record() { printf '%s %s %s\n' "$1" "$(state)" "$(probe frames)"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
QT_IM_MODULE=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for attempt in {1..40}; do
    [[ $(vk available 2>/dev/null) == true ]] && break
    sleep .1
done
test "$(vk available)" = true
client textCompanion
client topTextCompanion
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce

present() {
    local title=$1
    kad showCardLine
    sleep .4
    client focusText "$title"
    sleep .4
    kad showActive
    sleep 1
    client focusText "$title"
    sleep .5
    state | jq -e --arg t "$title" '.tracked == $t'
}
# The Active card on a 1280x800 tablet above a 64 px dock, and above none.
docked='{"x":10,"y":10,"width":1260,"height":716}'
open='{"x":10,"y":10,"width":1260,"height":780}'
is() { jq -e --argjson want "$2" '. == $want' <<<"$(frame "$1")"; }
# Its bottom edge stays above the dock's while the dock has stepped aside.
above_dock() { jq -e '.y + .height <= 726' <<<"$(frame "$1")"; }

# Signing in: the cards are presented before the dock has taken its room.
present "Keyboard reveal probe"
record before-dock
is "Keyboard reveal probe" "$open"
client dockSurface 64
sleep 1
record dock-arrived
state | jq -e '.workArea.height == 736'
is "Keyboard reveal probe" "$docked"
echo 'PASS: a card sized before the dock took its room is sized again when it does'

# The keys come up and the dock steps aside for them.
client focusText "Keyboard reveal probe"
sleep .5
kad raiseKeyboard
sleep 1
state | jq -e '.visible'
client dockReserve 0
sleep 1
record dock-yielded
state | jq -e '.workArea.height == 800'
above_dock "Keyboard reveal probe"
# Another card is chosen while the dock is still aside; nobody tapped its
# text, so the keys go with the focus.
client focusText "Keyboard top probe"
sleep .5
record other-card
state | jq -e '.tracked == "Keyboard top probe" and .workArea.height == 800'
above_dock "Keyboard top probe"
above_dock "Keyboard reveal probe"
echo 'PASS: a card chosen while the dock is aside for the keys stops at its edge'

# The dock takes its room back once the keys have gone, and nothing moves.
client dockReserve 64
sleep 1.5
record dock-back
state | jq -e '.visible == false and .workArea.height == 736'
is "Keyboard top probe" "$docked"
present "Keyboard reveal probe"
is "Keyboard reveal probe" "$docked"
echo 'PASS: every card stops at the dock once the keys have gone'

# A panel that leaves while no keys are up gives its room to the cards.
present "Keyboard top probe"
client dockReserve 0
sleep 1.5
record dock-left
state | jq -e '.visible == false and .workArea.height == 800'
is "Keyboard top probe" "$open"
echo 'PASS: room a panel gives up with no keys up goes to the Active card'

test "$(probe releaseRuntime)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
