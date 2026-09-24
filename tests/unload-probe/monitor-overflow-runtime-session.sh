#!/usr/bin/env bash
# A display that cannot own cards shows ordinary windows or one layout, never
# both (DECISIONS.md § A display without cards organizes everything it shows).
# A window its layout has no room for goes to the dock, minimized: never to the
# card display and never loose beside the layout. Picking it from the dock
# brings it back as an arrival. Switching Kadunce off returns every window to
# the desktop, minimized ones included.
#
# Needs the tablet fixture, so that a card display exists to be wrongly sent
# to. Each monitor window's minimum size lets two share the 1280x800 monitor
# and not three, whatever the layout's own pane count.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor overflow: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { kad workspaceContext | jq -r --arg c "$1" 'first(.applications[] | select(.title == $c)) | .windowId'; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, bento: [.displayContext.displays[] | {name, bentoActive}], apps: [.applications[] | {title, output, hasCard, minimized}]}')"
}
monitor='[.applications[] | select(.title | startswith("Monitor"))]'
# Nothing from the monitor became a card or crossed to the tablet.
stayed() { context "$monitor | all(.output == \"Virtual-1\" and (.hasCard | not))"; }
shown() { context "[$monitor[] | select(.minimized | not)] | length == $1"; }
waiting() { context "[$monitor[] | select(.minimized)] | length == $1"; }
layout() { context '.displayContext.displays[] | select(.name == "Virtual-1") | .bentoActive'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for name in 'Monitor A' 'Monitor B' 'Monitor C'; do client titledCompanion "$name" 440 500; done
sleep .8
for name in 'Monitor A' 'Monitor B' 'Monitor C'; do probe sendCaptionToOutput "$name" Virtual-1; done
sleep .4
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
cards=$(kad workspaceContext | jq '[.applications[] | select(.hasCard)] | length')

test "$(kad toggleBentoOnOutput Virtual-1)" = true
sleep 1
report organized
check 'organizing the monitor makes one layout' layout
check 'two of three fit and show' shown 2
check 'the third waits in the dock' waiting 1
check 'nothing left the monitor or became a card' stayed
check "the tablet's cards are unchanged" context "[.applications[] | select(.hasCard)] | length == $cards"

waiter=$(kad workspaceContext | jq -r "first($monitor[] | select(.minimized)) | .windowId")
probe minimizeWindow "$waiter" false
sleep 1
report picked
check 'the window picked from the dock shows' test "$(probe windowMinimized "$waiter")" = false
check 'a resident took its place in the dock' waiting 1
check 'still two shown, none loose' shown 2
check 'still nothing left the monitor' stayed

# KWin opens a window on the display under the pointer.
probe pointer 1900 400
client titledCompanion 'Monitor D' 440 500
sleep 1.2
report arrived
check 'a window opened on the monitor joins the layout' test "$(probe windowMinimized "$(idOf 'Monitor D')")" = false
check 'two shown, two waiting, none loose' shown 2
check 'two wait in the dock' waiting 2
check 'still nothing left the monitor' stayed

mine=$(kad workspaceContext | jq -r "first($monitor[] | select(.minimized | not) | select(.title != \"Monitor D\")) | .windowId")
probe minimizeWindow "$mine" true
sleep 1
report minimized
check 'a pane the person minimizes stays in the dock' test "$(probe windowMinimized "$mine")" = true
check 'and does not become a card on the tablet' stayed

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
for name in 'Monitor A' 'Monitor B' 'Monitor C' 'Monitor D'; do
    id=$(probe windowFacts | jq -r --arg c "$name" 'first(.[] | select(.caption == $c)) | .id // empty')
    [[ -n $id ]] || continue
    check "switching off returns $name to the desktop" test "$(probe windowMinimized "$id")" = false
done

if ((failures)); then echo "FAIL: monitor overflow: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a monitor layout sends what it cannot show to the dock, takes it back from there, and returns everything on switch-off'
