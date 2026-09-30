#!/usr/bin/env bash
# The keys come up for the first text field touched in a session (DECISIONS.md
# § The keyboard comes up for the text, not for focus). Under Plasma's
# touch-only setting the compositor shows the keys for a field only once a
# field has spoken to it while a touch was the last input, and never takes
# that back. This compositor starts with that setting and nothing touched. A
# window arrives with its application's own field focused; the first touch of
# the session lands in the window beside its fields, and the application
# focuses its field again, as a page does after a tap; then the first text
# field touched, the window's other one, brings the keys up. A field touched
# after that brings them too, and a click on a field never does.
#
# Needs the tablet fixture: the window is the Active card on the touch display.
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
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: keyboard first: $name" >&2; failures=$((failures + 1)); fi
}
up() { test "$(vk visible)" = true; }
down() { test "$(vk visible)" = false; }
tap() { probe down 1 "$1" "$2"; sleep .05; probe up 1; }
click() { probe pointer "$1" "$2"; probe button true; sleep .05; probe button false; }
lower() { probe hideKeyboard; sleep .8; }
# What decides each check: the keys, the focused field, which device KWin
# heard from last, and each time since the last report that the compositor
# showed or hid the keys, with the text cursor it reported then and where the
# last finger was.
report() {
    history=$(probe keysHistory)
    echo "  $1: keys $(vk visible) focus $(client focusedField) $(probe keyboardState | jq -c '{lastInput, cursor, panel}')"
    echo "    shown/hidden $(jq -c 'map({ms: (.ms | floor), visible, lastInput, cursorY: .cursor.y, fingerY: .finger.y})' <<<"$history")"
    probe watchKeys
}
never_shown() { jq -e 'all(.visible | not)' <<<"$history"; }
title='Keyboard pair probe'
for attempt in {1..40}; do qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe && break; sleep .1; done
for attempt in {1..40}; do [[ $(vk available 2>/dev/null) == true ]] && break; sleep .1; done
test "$(vk available)" = true || { echo 'FAIL: keyboard first: no input method' >&2; exit 1; }
echo "  mode $(vk mode) (1 is touch only)"
probe watchKeys
QT_IM_MODULE=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client pairTextCompanion
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
# The application focuses its top field itself, with nothing touched yet: the
# compositor does not show the keys at all.
client focusText "$title"
sleep 1.2
report 'the application focuses its field, nothing touched'
check 'nothing touched yet: a field the application focuses leaves the keys down' down
check 'nothing touched yet: the compositor never shows them' never_shown
check 'the window is the Active card' \
    jq -e --arg t "$title" '.cardStage.presentation == "active" and .focus.title == $t' <<<"$(kad workspaceContext)"

# Where each field stands on screen: the window's own coordinates of the two
# fields, placed by the text cursor the focused top field reports.
cursor=$(probe keyboardState | jq -c '.cursor')
frame=$(probe windowFacts | jq -c --arg t "$title" 'first(.[] | select(.caption == $t))')
read -r _ top_local < <(client fieldCentre "$title" topField)
read -r _ bottom_local < <(client fieldCentre "$title" bottomField)
offset=$(jq --argjson local "$top_local" '(.y + .height / 2) - $local | floor' <<<"$cursor")
fx=$(jq '.x + 40 | floor' <<<"$cursor")
top_y=$((top_local + offset)); bottom_y=$((bottom_local + offset))
mx=$(jq '.x + .width / 2 | floor' <<<"$frame"); my=$(jq '.y + .height / 2 | floor' <<<"$frame")
echo "  fields at x $fx: top y $top_y, bottom y $bottom_y; window middle $mx $my"

# The first touch of the session lands in the window beside its fields, and
# the application focuses its field again: the compositor may now show the
# keys, and they go back down, since nobody tapped the text.
tap "$mx" "$my"
sleep .3
client focusText "$title"
sleep 1.2
report 'the application focuses its field after a touch beside it'
check 'a field the application focuses after a touch elsewhere leaves the keys down' down

# The first text field touched in the session, the one the application did
# not focus.
tap "$fx" "$bottom_y"
sleep 1.2
report 'the first text field touched, the bottom one'
check 'the first touch on a text field focused that field' test "$(client focusedField)" = "$title/bottomField"
check 'the first text field touched in a session brings the keys up' up
lower

tap "$fx" "$top_y"
sleep 1.2
report 'a later touch, on the top field'
check 'a later touch on a text field focused that field' test "$(client focusedField)" = "$title/topField"
check 'a text field touched after the first brings them up too' up
lower

# Plasma's touch-only setting: a click never brings the keys, whether or not
# a touch has let them come up since the session began.
click "$fx" "$bottom_y"
sleep 1.2
report 'a click on the bottom field'
check 'a click on a text field leaves the keys down' down
lower

test "$(probe releaseRuntime)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: keyboard first: $failures checks failed" >&2; exit 1; fi
echo 'PASS: the first text field touched in a session brings the keys up, and a click never does'
