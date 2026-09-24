#!/usr/bin/env bash
# One window on a display without cards: a snap to a side gives it half of the
# display, and a snap to the top gives it the Active size, the full screen
# inside the gutter. A second window arriving organizes both into one layout
# (DECISIONS.md § A display without cards organizes everything it shows).
#
# Needs the tablet fixture: the carried window is a card on the tablet, and the
# monitor holds nothing else. Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor lone: $name" >&2; failures=$((failures + 1)); fi
}
twoPanes() { kad outputStageState | rg -q '^Virtual-1\|.*\|2$'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for scenario in side top; do
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .5
    probe contactObserve
    probe contactStart >/dev/null
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .3
    main=$(kad workspaceContext | jq -r '.applications[0].windowId')
    probe contactFocus
    probe pointer 500 350
    client armMove
    probe contactButton true
    sleep .1
    if [[ $scenario == side ]]; then probe contactMotion 2555 380; else probe contactMotion 1900 2; fi
    preview=$(kad nativeCarryState | jq -c '.destinationRect')
    echo "preview $scenario $preview"
    probe contactButton false
    sleep .8
    placed=$(probe windowGeometry "$main")
    echo "placed $scenario $placed"
    check "$scenario: the window lands where the preview showed" test "$placed" = "$preview"
    if [[ $scenario == side ]]; then
        check 'side: one window takes the right half of the monitor' \
            jq -e '.x > 1880 and .width > 560 and .width < 660' <<<"$placed"
        probe pointer 1900 400
        client titledCompanion 'Monitor B' 300 300
        sleep 1.2
        second=$(kad workspaceContext | jq -r 'first(.applications[] | select(.title == "Monitor B")) | .windowId')
        echo "organized $(probe windowGeometry "$main") $(probe windowGeometry "$second")"
        check 'side: a second window joins the layout' twoPanes
        check 'side: both show' test "$(probe windowMinimized "$second")" = false
    else
        check 'top: one window takes the full monitor inside the gutter' \
            jq -e '.x > 1280 and .x < 1310 and .width > 1200' <<<"$placed"
    fi
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .2
done
if ((failures)); then echo "FAIL: monitor lone: $failures checks failed" >&2; exit 1; fi
echo 'PASS: one window takes half of the monitor from a side and the Active size from the top; a second organizes both'
