#!/usr/bin/env bash
# Two windows share the monitor's layout; the person minimizes one there and
# picks it again from the dock. It waits in the dock, owned and listed, and
# comes back on the monitor as the same window beside the one that stayed
# (DECISIONS.md § A display without cards organizes everything it shows).
#
# The layout is made two ways: organized in place, and by carrying a card from
# the tablet onto the monitor's edge beside a window already there.
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
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor return: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
fact() { probe windowFacts | jq -e --arg id "$1" "first(.[] | select(.id == \$id)) | $2"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, output, hasCard, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c '[.[] | {caption, output, minimized, hidden, skipTaskbar, active, x, y, width, height}]')"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for scenario in organized carried; do
    # KWin opens a window on the display under the pointer.
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    client titledCompanion 'Monitor Other' 440 500
    [[ $scenario == organized ]] && client titledCompanion 'Monitor Zen' 440 500
    sleep .8
    probe sendCaptionToOutput unload-client Virtual-0
    probe sendCaptionToOutput 'Monitor Other' Virtual-1
    [[ $scenario == organized ]] && probe sendCaptionToOutput 'Monitor Zen' Virtual-1
    sleep .4
    probe contactObserve
    probe contactStart >/dev/null
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .8
    other=$(probe windowFacts | jq -r 'first(.[] | select(.caption == "Monitor Other")) | .id')
    if [[ $scenario == organized ]]; then
        zen=$(probe windowFacts | jq -r 'first(.[] | select(.caption == "Monitor Zen")) | .id')
        test "$(kad toggleBentoOnOutput Virtual-1)" = true
    else
        # The tablet's card is carried onto the monitor's right edge.
        zen=$(kad workspaceContext | jq -r 'first(.applications[] | select(.output == "Virtual-0")) | .windowId')
        probe contactFocus
        probe pointer 600 400
        client armMove
        probe contactButton true
        sleep .1
        probe contactMotion 2555 400
        sleep .2
        probe contactButton false
    fi
    sleep 1.2
    report "$scenario-laid-out"
    check "$scenario: both windows show on the monitor" \
        bash -c "$(declare -f fact probe); fact $zen '.output == \"Virtual-1\" and (.minimized | not)' && fact $other '.output == \"Virtual-1\" and (.minimized | not)'"

    probe minimizeWindow "$zen" true
    sleep 1
    report "$scenario-minimized"
    check "$scenario: the minimized window stays minimized" fact "$zen" '.minimized'
    check "$scenario: it stays on the monitor" fact "$zen" '.output == "Virtual-1"'
    check "$scenario: the dock still lists it" fact "$zen" '(.skipTaskbar | not) and (.hidden | not)'
    check "$scenario: it is not a card on the tablet" \
        context --arg id "$zen" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 0'
    check "$scenario: the other window still shows" fact "$other" '(.minimized | not) and (.hidden | not)'

    probe activateWindowId "$zen"
    sleep 1
    report "$scenario-picked"
    check "$scenario: picking it from the dock wakes it" fact "$zen" '(.minimized | not) and (.hidden | not)'
    check "$scenario: it comes back on the monitor" fact "$zen" '.output == "Virtual-1"'
    check "$scenario: the other window still shows" fact "$other" '(.minimized | not) and (.hidden | not)'

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .8
    report "$scenario-released"
    check "$scenario: switching off returns both windows awake" \
        bash -c "$(declare -f fact probe); fact $zen '.minimized | not' && fact $other '.minimized | not'"
    probe contactDrop
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: monitor return: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a window minimized from a two-window monitor layout waits in the dock and comes back when picked'
