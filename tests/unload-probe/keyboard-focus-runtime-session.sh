#!/usr/bin/env bash
# The keyboard comes up for a touch inside the text being typed into or for a
# pull on its handle, never because a card was focused. A GTK window asks for
# text input the way Ghostty does, so the compositor raises a keyboard
# whenever it gains focus: a card the stage brings forward keeps it down, and
# a tap inside the window brings it up.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
set -euo pipefail
trap 'echo "FAIL: keyboard focus runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
state() { probe keyboardState; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for attempt in {1..40}; do
    [[ $(vk available 2>/dev/null) == true ]] && break
    sleep .1
done
test "$(vk available)" = true
python3 "$(dirname "$0")/gtk-text.py" &
gtk_pid=$!
trap 'kill "$gtk_pid" 2>/dev/null || true' EXIT
sleep 3
state | jq -e '.tracked == "Keyboard GTK probe"'
# Without Kadunce, focusing the field raises a keyboard on its own.
state | jq -e '.visible == true'
probe hideKeyboard
sleep 1
echo 'PASS: this client has the compositor raise a keyboard whenever it gains focus'

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
kad showCardLine
sleep .6
kad showActive
sleep 1.2
state | jq -e '.tracked == "Keyboard GTK probe" and .visible == false'
echo 'PASS: a card the stage brings forward does not bring the keyboard with it'

frame=$(state | jq -c '.trackedFrame')
x=$(jq '(.x + .width / 2) | floor' <<<"$frame")
y=$(jq '(.y + 40) | floor' <<<"$frame")
probe down 1 "$x" "$y"
sleep .05
probe up 1
sleep 1
state | jq -e '.visible == true'
echo 'PASS: a tap inside the text brings the keyboard up'

probe hideKeyboard
sleep .6
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: keyboard focus rules measured'
