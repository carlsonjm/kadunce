#!/usr/bin/env bash
# The keys come up for a tap on the text (DECISIONS.md § The keyboard comes up
# for the text, not for focus), including a tap just after a card is chosen:
# the stage focusing the card it brings forward does not swallow the tap.
#
# Needs the tablet fixture: the windows are cards on the touch display.
set -euo pipefail
trap 'echo "FAIL: keyboard chosen runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
tap() { probe down "$1" "$2" "$3"; sleep .05; probe up "$1"; }
lower() { probe hideKeyboard; sleep .6; }
for attempt in {1..40}; do qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe && break; sleep .1; done
for attempt in {1..40}; do [[ $(vk available 2>/dev/null) == true ]] && break; sleep .1; done
test "$(vk available)" = true
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client topTextCompanion
sleep .5
client textCompanion
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
top=$(probe windowIdByCaption "Keyboard top probe")
other=$(probe windowIdByCaption "Keyboard reveal probe")
[[ -n $top && -n $other ]]
test "$(kad activateApplicationWindow "$top")" = true
sleep 1
client focusText 'Keyboard top probe'
sleep .6
lower
cursor=$(probe keyboardState | jq -c '.cursor')
cx=$(jq '.x + 40 | floor' <<<"$cursor"); cy=$(jq '.y + .height / 2 | floor' <<<"$cursor")
pick() {
    kad showCardLine
    sleep .8
    local rect
    rect=$(kad workspaceContext | jq -c --arg id "$1" '.applications[] | select(.windowId == $id) | .spreadRect')
    read -r px py <<<"$(jq -r '[(([.x,0]|max)+([.x+.width,1280]|min))/2,.y+.height/2] | map(floor) | @tsv' <<<"$rect")"
    tap 6 "$px" "$py"
}
pick "$other"
sleep 1
kad workspaceContext | jq -e --arg id "$other" '.cardStage.selectedCardId == $id'
pick "$top"
sleep .1
tap 5 "$cx" "$cy"
sleep 1
kad workspaceContext | jq -e --arg id "$top" '.cardStage.selectedCardId == $id'
test "$(vk visible)" = true
echo 'PASS: a tap on the text just after its card is chosen brings the keys up'
lower
