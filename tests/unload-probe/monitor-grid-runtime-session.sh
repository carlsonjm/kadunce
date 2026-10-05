#!/usr/bin/env bash
# Past the curated eight panes, the windows' minimum sizes decide how many a
# display without cards shows, not a count. Twelve 600x450 windows fill the
# 2560x1440 monitor as a four-by-three grid. A thirteenth opened there fits
# no grid of thirteen, so it takes one pane and that pane's window waits in
# the dock.
#
# Needs the tablet fixture and monitor-sized displays.
# Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor grid: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
monitor='[.applications[] | select(.output == "Virtual-1")]'
shown() { context "[$monitor[] | select(.minimized | not)] | length == $1"; }
waiting() { context "[$monitor[] | select(.minimized)] | length == $1"; }
panes() { kad outputStageState | rg -q "^Virtual-1\\|.*\\|$1\$"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c "[$monitor[] | {title, minimized}]")"
}
# Every shown pane on the monitor is at least its window's minimum.
sized() {
    local id geometry
    for id in $(kad workspaceContext | jq -r "$monitor[] | select(.minimized | not) | .windowId"); do
        geometry=$(probe windowGeometry "$id")
        jq -e '.width + 2 >= 600 and .height + 2 >= 450' <<<"$geometry" >/dev/null || return 1
    done
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
sleep .8
for n in {1..12}; do client titledCompanion "Grid $n" 600 450; done
sleep .8
for n in {1..12}; do probe sendCaptionToOutput "Grid $n" Virtual-1; done
sleep .4
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
test "$(kad toggleBentoOnOutput Virtual-1)" = true
sleep 1.2
report organized
check 'twelve windows show on the monitor' shown 12
check 'the layout holds twelve panes' panes 12
check 'no pane is smaller than its window' sized

# KWin opens a window on the display under the pointer.
probe pointer 3840 700
client titledCompanion 'Grid 13' 600 450
sleep 1.5
report arrived
check 'the thirteenth shows' context "[$monitor[] | select(.title == \"Grid 13\" and (.minimized | not))] | length == 1"
check 'still twelve panes' panes 12
check 'twelve show' shown 12
check 'the pane it took waits in the dock' waiting 1
check 'no pane is smaller than its window after the arrival' sized

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
kill "$client_pid"
wait "$client_pid" 2>/dev/null
if ((failures)); then echo "FAIL: monitor grid: $failures checks failed" >&2; exit 1; fi
echo 'PASS: minimum sizes, not a count of eight, decide how many panes the monitor shows'
