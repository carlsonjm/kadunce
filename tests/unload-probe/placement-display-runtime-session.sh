#!/usr/bin/env bash
# REQUESTS.md: a card asked onto another display goes there as the Active card
# carried there would. The middle of a monitor without a layout opens it there
# as an ordinary window, and a side edge begins that display's layout with the
# window already there. A monitor window asked onto the card display becomes
# the Active card there, and a pane asked there is refused. Needs the tablet
# fixture, so that a monitor exists beside it.
# Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: placement display: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, bento: [.displayContext.displays[] | {name, bentoActive}], apps: [.applications[] | {title, output, hasCard, minimized}]}')"
}
on() { context --arg t "$1" --arg o "$2" 'first(.applications[] | select(.title == $t)) | .output == $o and (.minimized | not)'; }
card() { context --arg t "$1" 'first(.applications[] | select(.title == $t)) | .hasCard'; }
notCard() { context --arg t "$1" 'first(.applications[] | select(.title == $t)) | .hasCard | not'; }
cards() { card Working && card Other; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .8
client colouredCompanion 'Working' 2e8b57 560 420
sleep .5
client colouredCompanion 'Other' 1e6fc8 600 440
sleep .8
probe contactObserve
probe contactStart >/dev/null
probe contactFocus >/dev/null
sleep .2
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
before=$(kad workspaceContext)
working=$(jq -r 'first(.applications[] | select(.title == "Working")) | .windowId' <<<"$before")
other=$(jq -r 'first(.applications[] | select(.title == "Other")) | .windowId' <<<"$before")
app=$(jq -r 'first(.applications[] | select(.title == "Working")) | .appId' <<<"$before")
report before
check 'both windows start as cards' cards

# The middle of the monitor: the card opens there as an ordinary window.
check 'the monitor middle aims at the display' test "$(kad aimPlacement 1920 400)" = display
check 'the card is placed on the monitor' \
    test "$(kad placeApplication "$app" "{$other}" 1920 400 middle)" = true
sleep 1.2
report middle
check 'it is on the monitor' on Other Virtual-1
check 'it is no longer a card' notCard Other
check 'the other card stays on the card display' on Working Virtual-0


# The monitor window asked back onto the card display becomes the Active card.
check 'the card display aims at the Active card' test "$(kad aimPlacement 640 400)" = card
check 'the monitor window is placed on the card display' \
    test "$(kad placeApplication "$app" "{$other}" 640 400 back)" = true
sleep 1.2
report back
check 'it is on the card display' on Other Virtual-0
check 'it is a card again' card Other
check 'it is the Active card' context --arg id "$other" \
    '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .windowId == $id)'
check 'the monitor middle aims at the display again' test "$(kad aimPlacement 1920 400)" = display
check 'the card goes back to the monitor' \
    test "$(kad placeApplication "$app" "{$other}" 1920 400 again)" = true
sleep 1.2
check 'it is on the monitor again' on Other Virtual-1

# The monitor's right edge: the card begins a layout with the window there.
check 'the monitor right edge aims at a side' test "$(kad aimPlacement 2555 400)" = right
check 'the card is placed at the side' \
    test "$(kad placeApplication "$app" "{$working}" 2555 400 side)" = true
sleep 1.5
report side
check 'it is on the monitor' on Working Virtual-1
check 'it is no longer a card' notCard Working
check 'the monitor shows a layout' \
    context '.displayContext.displays[] | select(.name == "Virtual-1") | .bentoActive'
check 'the window already there stayed' on Other Virtual-1
working_rect=$(probe windowGeometry "$working")
check 'it took the right side' jq -e '.x + .width > 2500' <<<"$working_rect"

# A pane asked onto the card display is refused, and keeps its layout.
check 'the card display aims at the Active card' test "$(kad aimPlacement 640 400)" = card
check 'a pane asked onto the card display is refused' \
    test "$(kad placeApplication "$app" "{$other}" 640 400 pane)" = false
sleep .4
check 'the refused pane stayed on the monitor' on Other Virtual-1

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: placement display: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card placed on another display by request goes there as a carried card would'
