#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3 and §7 with the keys: a text window minimized before
# Kadunce switches on, picked as the dock picks it, comes forward as the Active
# card, and the keys raised for it make room on it as for any Active card.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
set -euo pipefail
trap 'echo "FAIL: keyboard minimized runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
state() { probe keyboardState; }
frame() { probe frames | jq -c --arg t "$1" '.[$t]'; }
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
sleep 1
text=$(probe windowIdByCaption "Keyboard reveal probe")
[[ -n $text ]]
test "$(probe minimizeWindow "$text" true)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .6
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
test "$(probe activateWindowId "$text")" = true
sleep 1
kad workspaceContext | jq -e --arg id "$text" '.cardStage.selectedCardId == $id and .cardStage.presentation == "active"'
frame "Keyboard reveal probe" | jq -e '.width == 1260 and .height == 780'
echo 'PASS: a text window minimized before Kadunce, picked as the dock picks it, is the Active card'
client focusText "Keyboard reveal probe"
sleep .5
state | jq -e '.tracked == "Keyboard reveal probe"'
before=$(frame "Keyboard reveal probe")
kad raiseKeyboard
sleep 1
state | jq -e '.visible'
state | jq -e --argjson b "$before" '.trackedFrame.y == $b.y and .trackedFrame.width == $b.width
    and ((.trackedFrame.y + .trackedFrame.height + 10 - .panel.y) | fabs) <= 1'
echo 'PASS: the keys raised for it make room on it'
probe hideKeyboard
sleep .6
