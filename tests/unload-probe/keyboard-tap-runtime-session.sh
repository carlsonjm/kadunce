#!/usr/bin/env bash
# The keys come up only when the person asks for them (DECISIONS.md § The
# keyboard comes up for the text, not for focus). A tap on a field keeps them;
# the application focusing its field itself after a touch elsewhere, or a new
# window arriving with its field focused, leaves them down; a request through
# Kadunce raises them.
#
# Needs the tablet fixture: the window is a card on the touch display.
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
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: keyboard tap: $name" >&2; failures=$((failures + 1)); fi
}
up() { test "$(vk visible)" = true; }
down() { test "$(vk visible)" = false; }
tap() { probe down "$1" "$2" "$3"; sleep .05; probe up "$1"; sleep .3; }
lower() { probe hideKeyboard; sleep .6; }
report() { echo "  $1: keys $(vk visible) cursor $(probe keyboardState | jq -c '.cursor')"; }
for attempt in {1..40}; do qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe && break; sleep .1; done
for attempt in {1..40}; do [[ $(vk available 2>/dev/null) == true ]] && break; sleep .1; done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client topTextCompanion
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
client focusText 'Keyboard top probe'
sleep 1
lower
cursor=$(probe keyboardState | jq -c '.cursor')
report 'the field, focused'
cx=$(jq '.x + 40 | floor' <<<"$cursor"); cy=$(jq '.y + .height / 2 | floor' <<<"$cursor")
frame=$(probe windowFacts | jq -c 'first(.[] | select(.caption == "Keyboard top probe"))')
mx=$(jq '.x + .width / 2 | floor' <<<"$frame"); my=$(jq '.y + .height / 2 | floor' <<<"$frame")

tap 1 "$cx" "$cy"
sleep 1
report 'a tap on the field'
check 'a tap on the line of the text brings the keys up' up
lower

tap 3 "$mx" "$my"
client topTextCompanion
sleep 1.5
report 'a new window'
check 'a new window with its field focused leaves them down' down
lower

kad raiseKeyboard
sleep 1.2
report 'a request'
check 'a request through Kadunce brings them up' up
lower

if ((failures)); then echo "FAIL: keyboard tap: $failures checks failed" >&2; exit 1; fi
echo 'PASS: the keys come up for a tap on the text and a request, and for nothing else'
