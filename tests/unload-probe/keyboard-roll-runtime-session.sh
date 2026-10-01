#!/usr/bin/env bash
# The Active card follows the keys on every frame they move, rising, put away
# by their Hide key, put away by an application, and leaving when typing ends,
# for a client that draws at once and for one as slow as a browser. The gap
# above the keys stays the gutter, and the client is asked for a new size once
# a motion rather than once a frame.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
set -euo pipefail
trap 'echo "FAIL: keyboard roll runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
state() { probe keyboardState; }
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
# As long to draw as a browser laying a page out again.
client slowTextCompanion 40
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
raise() {
    client focusText "$title"
    sleep .5
    kad raiseKeyboard
    sleep 1
    state | jq -e '.visible'
}
# Records every frame painted while the keys move, then checks the card: the
# gap seen is the gutter once the client has had `catch_up` milliseconds, the
# client was asked for one size, and the keys were seen in `steps` places.
measure() {
    local label=$1 catch_up=$2 steps=$3; shift 3
    probe watchRoom "$title"
    "$@"
    sleep 1.5
    local history
    history=$(probe roomHistory)
    # Kept beside the session log, frame by frame, for reading a failure.
    printf '%s\n' "$history" >"$(dirname "$XDG_RUNTIME_DIR")/roll-${title// /-}-$label.json"
    printf 'roll %s %s ' "$title" "$label"
    python3 "$(dirname "$0")/roll-check.py" 10 "$catch_up" 1 "$steps" <<<"$history"
}
put_away() {
    local keys x y
    keys=$(state | jq '.panel')
    # The Hide key, in the bottom row right of 123.
    x=$(jq '.x + .width * 0.77 | floor' <<<"$keys"); y=$(jq '.y + .height * 0.87 | floor' <<<"$keys")
    probe down 1 "$x" "$y"; sleep .05; probe up 1
}

# A client that draws at once never shows more than the gutter; one as slow as
# a browser has caught up within a tenth of a second.
for title in "Keyboard reveal probe" "Keyboard slow probe"; do
    catch_up=0
    [[ $title == "Keyboard slow probe" ]] && catch_up=100
    present "$title"
    # The client focused its own field on the way in, and Kadunce kept those
    # keys down. Asked for now, they still rise rather than appear at rest.
    raise_now() { kad raiseKeyboard; }
    measure rise "$catch_up" 8 raise_now
    raise
    measure hide-key "$catch_up" 8 put_away
    state | jq -e '.visible == false'
    # Keys an application puts away, and keys that go when typing ends, slide
    # out as the Hide key's do rather than vanishing.
    raise
    measure hide "$catch_up" 8 probe hideKeyboard
    raise
    measure ends "$catch_up" 8 client leaveText "$title"
    sleep .5
    test "$(state | jq '.visible')" = false
done
echo 'PASS: the card follows the keys every frame and its client is asked once a motion'
