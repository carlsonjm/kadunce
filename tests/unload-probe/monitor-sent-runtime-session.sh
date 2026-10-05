#!/usr/bin/env bash
# A window KWin sends to the card display some other way than a display
# change, as its window-to-screen shortcut does, becomes a card there. The
# card display hides any window that is not a card, so left alone it could
# not be seen. The window it is sent beside keeps its card.
#
# Needs the tablet fixture, so that a card display exists beside the monitor.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor sent: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { kad workspaceContext | jq -r --arg c "$1" 'first(.applications[] | select(.title == $c)) | .windowId'; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, output, hasCard, minimized}]}')"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
cards=$(kad workspaceContext | jq '[.applications[] | select(.hasCard)] | length')
check 'the card display holds a card' test "$cards" -ge 1

# KWin opens a window on the display under the pointer. The monitor shows no
# layout, so the window is an ordinary one there.
probe pointer 1900 400
client titledCompanion 'Sent Window' 440 500
sleep 1.2
sent=$(idOf 'Sent Window')
report opened
check 'the window opened on the monitor as an ordinary window' \
    context ".applications[] | select(.title == \"Sent Window\") | .output == \"Virtual-1\" and (.hasCard | not)"

probe sendCaptionToOutput 'Sent Window' Virtual-0
sleep 1.5
report sent
check 'the sent window is on the card display' \
    context ".applications[] | select(.title == \"Sent Window\") | .output == \"Virtual-0\""
check 'the sent window became a card' \
    context ".applications[] | select(.title == \"Sent Window\") | .hasCard"
check 'the sent window is not hidden' test "$(probe windowMinimized "$sent")" = false
check 'the cards already there kept theirs' \
    context "[.applications[] | select(.hasCard)] | length == $((cards + 1))"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
if ((failures)); then echo "FAIL: monitor sent: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a window KWin sends to the card display becomes a card there'
