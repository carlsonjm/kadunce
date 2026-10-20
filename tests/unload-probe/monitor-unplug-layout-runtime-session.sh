#!/usr/bin/env bash
# A layout on a monitor that is unplugged gives its windows back on the tablet
# as a card gives one back: carried there, the same share of the way across
# and down they stood on the monitor, never pinned to the tablet's edge
# (ReleasePlace.h). Switched off afterwards, each window stands where it came
# back, whole on the tablet.
#
# Needs the tablet fixture: a 1280x800 tablet with the monitor to its right.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor unplug layout: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
facts() { probe windowFacts | jq -e --arg c "$1" "first(.[] | select(.caption == \$c)) | $2"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{bento: [.displayContext.displays[] | {name, bentoActive}], apps: [.applications[] | {title, output, hasCard, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height})')"
}
layout() { context '.displayContext.displays[] | select(.name == "Virtual-1") | .bentoActive'; }
# Whole on the tablet, and clear of both its side edges.
inside() { facts "$1" '.output == "Virtual-0" and .x > 8 and .x + .width < 1272 and .y >= 0 and .y + .height <= 800'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for name in 'Monitor A' 'Monitor B'; do client titledCompanion "$name" 440 500; done
sleep .8
for name in 'Monitor A' 'Monitor B'; do probe sendCaptionToOutput "$name" Virtual-1; done
sleep .4
# Both stand well inside the monitor, a third and two thirds of the way across:
# pinned to an edge, either would touch the tablet's right side.
probe placeCaption 'Monitor A' 1560 150
probe placeCaption 'Monitor B' 2000 200
sleep .4
report placed
check 'placed: both stand on the monitor' facts 'Monitor B' '.output == "Virtual-1" and .x == 2000'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
test "$(kad toggleBentoOnOutput Virtual-1)" = true
sleep 1
report organized
check 'organizing the monitor makes one layout' layout

kscreen-doctor output.Virtual-1.disable >/dev/null 2>&1
sleep 2
report unplugged
check 'unplugged: both windows came to the tablet' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and (.caption | startswith(\"Monitor\")))) | length == 2 and all(.output == \"Virtual-0\")'"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: the first stands inside the tablet, off its edges' inside 'Monitor A'
check 'switched off: the second stands inside the tablet, off its edges' inside 'Monitor B'
check 'switched off: they keep their order across' \
    bash -c "a=\$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq '[.[] | select(.caption == \"Monitor A\")][0].x'); b=\$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq '[.[] | select(.caption == \"Monitor B\")][0].x'); (( \${a%.*} < \${b%.*} ))"
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\")) | all(.onDisplay and (.hidden | not))'"
kscreen-doctor output.Virtual-1.enable >/dev/null 2>&1
if (( failures > 0 )); then
    echo "FAIL: monitor unplug layout: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: a layout on an unplugged monitor gives its windows back carried onto the tablet, not pinned to its edge'
