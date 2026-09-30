#!/usr/bin/env bash
# The keys come up for a text box tapped right after its card is chosen
# (DECISIONS.md § The keyboard comes up for the text, not for focus). A card
# chosen in Spread comes forward with its application's own field focused, and
# keys the compositor shows for that focus go back down; a tap on a text box
# in the card a moment later is the person asking, however soon it follows the
# choice and whichever of the card's fields it lands on.
#
# Needs the tablet fixture: only a display that can own cards presents Spread.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: keyboard choose: $name" >&2; failures=$((failures + 1)); fi
}
up() { test "$(vk visible)" = true; }
down() { test "$(vk visible)" = false; }
now() { echo $(($(date +%s%N) / 1000000)); }
lower() { probe hideKeyboard; sleep .6; }
presentation() { kad workspaceContext | jq -r '.cardStage.presentation'; }
title='Keyboard pair probe'
for attempt in {1..40}; do qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe && break; sleep .1; done
for attempt in {1..40}; do [[ $(vk available 2>/dev/null) == true ]] && break; sleep .1; done
test "$(vk available)" = true || { echo 'FAIL: keyboard choose: no input method' >&2; exit 1; }
echo "  mode $(vk mode) (1 is touch only)"
QT_IM_MODULE=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client pairTextCompanion
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
client focusText "$title"
sleep .8

# Where the card's fields stand once it is the Active card: the window's own
# coordinates of each field, placed where the window stands.
frame=$(probe windowFacts | jq -c --arg t "$title" 'first(.[] | select(.caption == $t))')
read -r _ top_local < <(client fieldCentre "$title" topField)
read -r _ bottom_local < <(client fieldCentre "$title" bottomField)
fx=$(jq '.x + 50 | floor' <<<"$frame"); fy=$(jq '.y | floor' <<<"$frame")
top_y=$((fy + top_local)); bottom_y=$((fy + bottom_local))
echo "  Active frame $frame; fields at x $fx: top y $top_y, bottom y $bottom_y"

# Chooses the card in Spread by a tap on it, with its application's own focus
# back in the top field, then taps a field of the chosen card `delay`
# milliseconds after the finger left the card.
choose_then_tap() {
    local delay=$1 field=$2 y=$3 label=$4
    kad showCardLine
    sleep .5
    client focusText "$title"
    sleep .3
    local rect cx cy
    rect=$(kad workspaceContext | jq -c --arg t "$title" 'first(.applications[] | select(.title == $t)) | .spreadRect')
    cx=$(jq '.x + .width / 2 | floor' <<<"$rect"); cy=$(jq '.y + .height / 2 | floor' <<<"$rect")
    probe watchKeys
    probe down 1 "$cx" "$cy"; sleep .05; probe up 1
    local chosen tapped before=''
    chosen=$(now)
    if ((delay >= 1000)); then
        sleep .9
        echo "  chosen, before any tap: keys $(vk visible), $(presentation)"
        check "$label: a card chosen by a tap does not bring the keys with it" down
    fi
    # The keys' panel as the tap comes: a frame of 2 is the Keyboard's window
    # up with its keys below the edge, 0 is no window.
    ((delay >= 300)) && before=$(probe keyboardState | jq -c '.panel.height')
    while (($(now) < chosen + delay)); do sleep .01; done
    probe down 2 "$fx" "$y"
    tapped=$(now)
    sleep .05; probe up 2
    sleep 1
    echo "  $label: card at $rect tapped at $cx $cy; field tapped $((tapped - chosen)) ms after, panel height then ${before:-unread}; keys $(vk visible) focus $(client focusedField) $(presentation)"
    echo "    shown/hidden $(probe keysHistory | jq -c 'map({ms: (.ms | floor), visible, lastInput, cursorY: .cursor.y, fingerY: .finger.y})')"
    check "$label: the tap focused the $field field" test "$(client focusedField)" = "$title/${field}Field"
    check "$label: the keys come up" up
    lower
}

# The field the application focused, from well after the card has settled to
# as soon as a quick hand follows the choice.
choose_then_tap 1500 top "$top_y" 'the focused field 1.5 s after the card is chosen'
choose_then_tap 300 top "$top_y" 'the focused field 0.3 s after'
choose_then_tap 100 top "$top_y" 'the focused field 0.1 s after'
# The card's other field, where the application's cursor is not.
choose_then_tap 1500 bottom "$bottom_y" 'the other field 1.5 s after'
choose_then_tap 1000 bottom "$bottom_y" 'the other field 1 s after'
choose_then_tap 600 bottom "$bottom_y" 'the other field 0.6 s after'
choose_then_tap 300 bottom "$bottom_y" 'the other field 0.3 s after'

test "$(probe releaseRuntime)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: keyboard choose: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a text box tapped right after its card is chosen brings the keys up'
