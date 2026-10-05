#!/usr/bin/env bash
# CARD-LIFECYCLE.md §5 and §8: a card released into a side edge of a full
# layout takes that side, and the pane that yields is the one on that side.
# The pane on the other side keeps its window and its side. Which window was
# added to the layout first decides nothing.
#
# The monitor holds a two-pane layout, organized in place, and the tablet's
# Active card is carried by pointer onto the monitor's left edge, then, from
# the same start, onto its right edge. Each is run with the layout's windows
# opened in both orders, so an answer that follows the order the panes were
# added cannot pass.
#
# Needs the tablet fixture, so that a card display exists beside the monitor.
# Each monitor window's minimum size lets two share the 1280x800 monitor and
# not three, whatever the layout's own pane count.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor side: $name" >&2; failures=$((failures + 1)); fi
}
fact() { probe windowFacts | jq -e --arg id "$1" "first(.[] | select(.id == \$id)) | $2"; }
idOf() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
# Shown on the monitor, in its layout.
shows() { fact "$1" '.output == "Virtual-1" and (.minimized | not) and (.hidden | not)'; }
twoPanes() { kad outputStageState | grep -q '^Virtual-1|.*|2$'; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '[.applications[] | {title, output, hasCard, minimized}]')"
    echo "facts $1 $(probe windowFacts | jq -c '[.[] | select(.normal) | {caption, output, minimized, x, y, width, height}]')"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for order in first second; do
for edge in left right; do
    # KWin opens a window on the display under the pointer.
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    if [[ $order == first ]]; then names=('Side One' 'Side Two'); else names=('Side Two' 'Side One'); fi
    for name in "${names[@]}"; do client titledCompanion "$name" 440 500; sleep .3; done
    sleep .5
    probe sendCaptionToOutput unload-client Virtual-0
    for name in "${names[@]}"; do probe sendCaptionToOutput "$name" Virtual-1; done
    sleep .4
    probe contactObserve
    probe contactStart >/dev/null
    probe contactFocus >/dev/null
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .8
    main=$(idOf unload-client)
    test "$(kad toggleBentoOnOutput Virtual-1)" = true
    sleep 1.2
    report "$order-$edge-organized"
    one=$(idOf 'Side One')
    two=$(idOf 'Side Two')
    read -r leftPane rightPane < <(probe windowFacts | jq -r --arg a "$one" --arg b "$two" \
        '[.[] | select(.id == $a or .id == $b)] | sort_by(.x) | "\(.[0].id) \(.[1].id)"')
    if [[ $edge == left ]]; then yields=$leftPane stays=$rightPane x=1283; else yields=$rightPane stays=$leftPane x=2556; fi
    staysAt=$(fact "$stays" '.x')
    echo "$order $edge: left pane $(fact "$leftPane" .caption) right pane $(fact "$rightPane" .caption)"
    check "$order $edge: the monitor holds a two-pane layout" twoPanes

    # The tablet's Active card, carried by its window onto the monitor's edge.
    probe contactFocus >/dev/null
    probe pointer 600 400
    client armMove
    probe contactButton true
    sleep .1
    probe contactMotion 1900 400
    sleep .15
    probe contactMotion "$x" 400
    sleep .3
    echo "$order $edge: at the edge $(kad nativeCarryState | jq -c '{destination, destinationPreview, destinationRect}')"
    probe contactButton false
    sleep 1.5
    report "$order-$edge-dropped"
    echo "$order $edge: yielding should be $(fact "$yields" .caption), staying $(fact "$stays" .caption) at x $staysAt, now at x $(fact "$stays" .x)"
    check "$order $edge: the carried card shows on the monitor" shows "$main"
    check "$order $edge: it takes the $edge side" \
        fact "$main" "if \"$edge\" == \"left\" then .x < $staysAt else .x > $staysAt end"
    check "$order $edge: the $edge pane yields" \
        fact "$yields" '.output != "Virtual-1" or .minimized or .hidden'
    check "$order $edge: the other pane keeps its window" shows "$stays"
    check "$order $edge: the other pane keeps its side" \
        fact "$stays" "(.minimized | not) and if \"$edge\" == \"left\" then .x > 1600 else .x < 1600 end"

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    probe contactDrop
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done
done

if ((failures)); then echo "FAIL: monitor side: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card released into a side of a full layout replaces the pane on that side'
