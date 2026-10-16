#!/usr/bin/env bash
# CARD-LIFECYCLE.md §2, Desktop: the Active card a person maximizes asks to
# fill the screen, so it goes to the desktop alone and the rest wait aside, as
# a window let go at the bottom edge does. An application opened after it is
# the Active card over the desktop. Asking for the maximized window then, as
# the dock or a task switcher does, shows the desktop again with it there,
# drawn and taking touches, never left behind that card; choosing the card
# brings it back; and release gives the window back maximized. Needs the
# tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: fill return: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, hasCard, selected}]}')"
}
isCard() { context --arg id "$1" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 1'; }
notCard() { context --arg id "$1" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 0'; }
activeIs() { context --arg id "$1" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'; }
# Which window a touch in the middle of the tablet reaches.
middle() { probe windowAt 640 400 | jq -r '.target.caption // ""'; }
# Whether KWin draws a window at all: a card held aside is hidden, so the
# window under it is what shows.
shown() { probe windowFacts | jq -e --arg c "$1" 'first(.[] | select(.caption == $c)) | (.hidden | not) and (.minimized | not)'; }
hidden() { probe windowFacts | jq -e --arg c "$1" 'first(.[] | select(.caption == $c)) | .hidden'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
# KWin opens a window on the display under the pointer.
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client colouredCompanion 'Fills the screen' 2e8b57 560 420
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
fills=$(probe windowIdByCaption 'Fills the screen')
probe activateWindowId "$fills" >/dev/null
# Past its opening, so the maximize is a person's, not its application's.
sleep 2.2
check "the window is the Active card" activeIs "$fills"
report before

probe maximizeCaption 'Fills the screen' >/dev/null
sleep 1
report maximized
check "maximized, it goes to the desktop" context '.cardStage.presentation == "desktop"'
check "it is no longer a card" notCard "$fills"
check "it fills the screen" jq -e '.width >= 1270' <<<"$(probe windowGeometry "$fills")"

# An application opened now is the Active card over the desktop.
client colouredCompanion 'Opens after' 0b5394 560 420
sleep 1.2
after=$(probe windowIdByCaption 'Opens after')
report opened
check "the application opened after it is the Active card" activeIs "$after"
check "the maximized window stays off the cards" notCard "$fills"

# Asked for as the dock does, the maximized window is shown on the desktop.
probe activateWindowId "$fills" >/dev/null
sleep .8
report asked
check "asking for the maximized window shows the desktop" context '.cardStage.presentation == "desktop"'
check "the maximized window is drawn" shown 'Fills the screen'
check "the later card is out of sight" hidden 'Opens after'
check "the maximized window takes a touch in the middle" test "$(middle)" = 'Fills the screen'
check "the later application is still a card, held aside" isCard "$after"

# Chosen, the card comes back over it, and asking again shows it again.
probe activateWindowId "$after" >/dev/null
sleep .8
report chosen
check "choosing the card shows it Active again" activeIs "$after"
check "the card is drawn again" shown 'Opens after'
check "the card takes a touch in the middle" test "$(middle)" = 'Opens after'
probe activateWindowId "$fills" >/dev/null
sleep .8
report asked-again
check "asking again shows the desktop again" context '.cardStage.presentation == "desktop"'
check "the maximized window is drawn again" shown 'Fills the screen'
check "the later card is out of sight again" hidden 'Opens after'
check "the maximized window takes a touch in the middle again" test "$(middle)" = 'Fills the screen'

test "$(probe releaseRuntime)" = true
sleep .5
check "release gives the window back maximized" jq -e '.width >= 1270' <<<"$(probe windowGeometry "$fills")"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: fill return: $failures checks failed" >&2; exit 1; fi
echo "PASS: a maximized card goes to the desktop, and asking for it under a card opened later shows it there"
