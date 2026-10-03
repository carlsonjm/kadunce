#!/usr/bin/env bash
# Renaming a Table tab with the Shuffle Keyboard while an application's field
# has the text, as a terminal or an editor does. A tab held in Table's menu
# bar brings the keys up; each key tapped reaches the name, never the field;
# Enter keeps the name; and once Table closes the field takes the keys again.
#
# Needs the tablet fixture: the touches are on the touch display.
set -euo pipefail
trap 'echo "FAIL: keyboard table $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
table() { kad tableState | jq -r "$1"; }
at() { kad tableState | jq -r "$1 | floor"; }
tap() { probe down "$1" "$2" "$3"; sleep .05; probe up "$1"; sleep 1; }
for attempt in {1..40}; do qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe && break; sleep .1; done
for attempt in {1..40}; do [[ $(vk available 2>/dev/null) == true ]] && break; sleep .1; done
test "$(vk available)" = true
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
probe hideKeyboard
sleep .6
cursor=$(probe keyboardState | jq -c '.cursor')
cx=$(jq '.x + 40 | floor' <<<"$cursor"); cy=$(jq '.y + .height / 2 | floor' <<<"$cursor")
test -z "$(client fieldText 'Keyboard top probe')"

# Table as the menu bar its key opens.
probe pointer 600 400
kad toggleTable
sleep .6
test "$(table '.open')" = true
test "$(table '.sticky')" = true
probe down 91 "$(at '.layout.tabs[0][0]')" "$(at '.layout.tabs[0][1]')"
sleep .9
probe up 91
sleep 1.2
test "$(table '.renaming')" = 0
test "$(table '.typing')" = true
test "$(table '.textFocusedApplication')" = false
test "$(vk visible)" = true
echo "PASS: a tab held in Table's menu bar is renamed, and the keys come up for it"

# A letter on the left of the second row.
state=$(probe keyboardState)
kx=$(jq '(.panel.x + .panel.width * 0.2) | floor' <<<"$state")
ky=$(jq '(.panel.y + .panel.height * 0.45) | floor' <<<"$state")
before=$(table '.renameText')
tap 61 "$kx" "$ky"
first=$(table '.renameText')
tap 62 "$kx" "$ky"
second=$(table '.renameText')
printf 'typed %s, then %s, then %s; the field holds "%s"\n' "$before" "$first" "$second" "$(client fieldText 'Keyboard top probe')"
test "${#first}" = 1
test "${#second}" = 2
test -z "$(client fieldText 'Keyboard top probe')"
echo "PASS: each key reaches the name, the chosen old one giving way, and never the application's field"

probe key 28 0
sleep .6
test "$(table '.renaming')" = -1
test "$(table '.workspaces[0].name')" = "$second"
test "$(table '.named | length')" = 1
probe key 1 0
sleep .6
test "$(table '.open')" = false
echo 'PASS: Enter keeps the name'

# The field has the keys again: a tap on it, then a letter.
tap 63 "$cx" "$cy"
test "$(vk visible)" = true
state=$(probe keyboardState)
kx=$(jq '(.panel.x + .panel.width * 0.2) | floor' <<<"$state")
ky=$(jq '(.panel.y + .panel.height * 0.45) | floor' <<<"$state")
tap 64 "$kx" "$ky"
printf 'the field holds "%s"\n' "$(client fieldText 'Keyboard top probe')"
test -n "$(client fieldText 'Keyboard top probe')"
echo "PASS: once Table closes, the application's field takes the keys again"
