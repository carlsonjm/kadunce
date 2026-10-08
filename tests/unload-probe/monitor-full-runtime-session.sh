#!/usr/bin/env bash
# A card carried onto a full layout on a display without cards takes one slot,
# and that slot's window waits in the dock (DECISIONS.md § A display without
# cards organizes everything it shows). The layout keeps every other pane.
#
# Needs the tablet fixture and monitor-sized displays: eight 600x450 windows
# fill the 2560x1440 monitor at the layout's pane count, and the carried card,
# at least 940x500, fits only the pattern's large slot. That slot is on the left
# of the library pattern, so a snap to the right edge has to bring it there.
# The pane that waits after the side snap is the one that occupied the right
# edge where the contact was, its upper half (CARD-LIFECYCLE.md §5), and the
# preview covers it while dragging. Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor full: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
monitor='[.applications[] | select(.output == "Virtual-1")]'
shown() { context "[$monitor[] | select(.minimized | not)] | length == $1"; }
waiting() { context "[$monitor[] | select(.minimized)] | length == $1"; }
panes() { kad outputStageState | rg -q "^Virtual-1\\|.*\\|$1\$"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c "[$monitor[] | {title, minimized}]")"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for scenario in side top; do
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    for n in 1 2 3 4 5 6 7 8; do client titledCompanion "Monitor $n" 600 450; done
    sleep .8
    for n in 1 2 3 4 5 6 7 8; do probe sendCaptionToOutput "Monitor $n" Virtual-1; done
    sleep .4
    probe contactObserve
    probe contactStart >/dev/null
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .8
    main=$(kad workspaceContext | jq -r 'first(.applications[] | select(.output == "Virtual-0")) | .windowId')
    client minimumSizeHint 940 500
    test "$(kad toggleBentoOnOutput Virtual-1)" = true
    sleep 1.2
    report "$scenario-organized"
    check "$scenario: eight windows fill the monitor" shown 8
    check "$scenario: the layout holds eight panes" panes 8

    occupant=
    if [[ $scenario == side ]]; then
        # The pane against the monitor's right edge highest up, which the
        # contact's upper half names.
        occupant=$(for id in $(kad workspaceContext | jq -r "$monitor[] | .windowId"); do
                probe windowGeometry "$id" | jq -c --arg id "$id" '. + {id: $id}'
            done | jq -rs 'map(select(.width > 0)) | (map(.x + .width) | max) as $edge
                | map(select(.x + .width >= $edge - 1)) | min_by(.y) | .id')
        echo "occupant $occupant $(probe windowGeometry "$occupant")"
        check 'side: a pane occupies the right edge' test -n "$occupant"
    fi

    probe contactFocus
    probe pointer 700 500
    client armMove
    probe contactButton true
    sleep .1
    if [[ $scenario == side ]]; then probe contactMotion 5115 700; else probe contactMotion 3840 2; fi
    sleep .2
    preview=$(kad nativeCarryState | jq -c '.destinationRect')
    echo "preview $scenario $preview"
    check "$scenario: the edge shows a preview" jq -e '. != null and .width > 0' <<<"$preview"
    check "$scenario: the preview is one slot, not the whole monitor" \
        jq -e '.width < 2000 or .height < 1100' <<<"$preview"
    if [[ -n $occupant ]]; then
        held=$(probe windowGeometry "$occupant")
        check 'side: the preview covers the pane that will yield' jq -en \
            --argjson a "$preview" --argjson b "$held" \
            '$a.x < $b.x + $b.width and $a.x + $a.width > $b.x
             and $a.y < $b.y + $b.height and $a.y + $a.height > $b.y'
    fi
    probe contactButton false
    sleep 1.5
    report "$scenario-dropped"
    placed=$(probe windowGeometry "$main")
    echo "placed $scenario $placed"
    check "$scenario: the carried window lands where the preview showed" test "$placed" = "$preview"
    if [[ $scenario == side ]]; then
        check 'side: the carried window lands against the right edge' \
            jq -e '.x + .width > 5100' <<<"$placed"
    fi
    check "$scenario: the carried window shows on the monitor" \
        test "$(probe windowMinimized "$main")" = false
    check "$scenario: still eight panes" panes 8
    check "$scenario: eight show" shown 8
    check "$scenario: the one it replaced waits in the dock" waiting 1
    if [[ -n $occupant ]]; then
        check 'side: the pane that occupied the right edge is the one waiting' \
            test "$(probe windowMinimized "$occupant")" = true
    fi

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done
if ((failures)); then echo "FAIL: monitor full: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card carried onto a full monitor layout takes one slot, and that slot waits in the dock'
