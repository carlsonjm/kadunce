#!/usr/bin/env bash
# CARD-LIFECYCLE.md §10 and §11 on the monitor: a pane of the monitor's layout
# carried by pointer down to the monitor's bottom edge, into the band the dock
# keeps, and let go there. Wherever it lands it stays visible: shown, not
# minimized, and on a display.
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
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor bottom: $name" >&2; failures=$((failures + 1)); fi
}
fact() { local id=$1; shift; local query=${*: -1}; probe windowFacts | jq -e --arg id "$id" "${@:1:$#-1}" "first(.[] | select(.id == \$id)) | $query"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, output, hasCard, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c '[.[] | select(.normal) | {caption, output, minimized, hidden, skipTaskbar, x, y, width, height}]')"
}
# Shown, not minimized, and wholly on one of the two displays.
visible() {
    fact "$1" '(.minimized | not) and (.hidden | not)
        and ((.output == "Virtual-0" and .x >= 0 and .x + .width <= 1280)
             or (.output == "Virtual-1" and .x >= 1280 and .x + .width <= 2560))
        and .y >= 0 and .y + .height <= 800'
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for release in band edge; do
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    client titledCompanion 'Monitor Other' 440 500
    sleep .8
    probe sendCaptionToOutput unload-client Virtual-1
    probe sendCaptionToOutput 'Monitor Other' Virtual-1
    sleep .4
    report "$release-before"
    probe contactObserve
    probe contactStart >/dev/null
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .8
    main=$(probe windowFacts | jq -r 'first(.[] | select(.caption == "unload-client")) | .id')
    other=$(probe windowFacts | jq -r 'first(.[] | select(.caption == "Monitor Other")) | .id')
    test "$(kad toggleBentoOnOutput Virtual-1)" = true
    sleep 1.2
    report "$release-laid-out"
    check "$release: the layout holds both windows" bash -c "$(declare -f fact visible probe); visible $main && visible $other"

    # The pane is carried from its middle straight down and let go in the
    # dock's band, or at the very last row.
    read -r px py < <(probe windowGeometry "$main" | jq -r '"\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
    read -r pane_w pane_h < <(probe windowGeometry "$main" | jq -r '"\(.width | floor) \(.height | floor)"')
    [[ $release == band ]] && bottom=775 || bottom=799
    probe contactFocus >/dev/null
    probe pointer "$px" "$py"
    client armMove
    probe contactButton true
    sleep .15
    probe contactMotion "$px" 600
    sleep .15
    probe contactMotion "$px" "$bottom"
    sleep .3
    echo "$release at the bottom: $(kad nativeCarryState | jq -c '.')"
    probe contactButton false
    sleep 1.2
    echo "carry trace: $(kad nativeMoveTrace | jq -sc 'map(.event) | .[-5:]')"
    report "$release-released"
    check "$release: the carried window stays visible" visible "$main"
    # A pane let go at the bottom edge lands at its floating size; this one
    # was maximized before the layout and KWin holds no floating size for it,
    # so it keeps the size it was carried at, as far as four fifths of the
    # display allows. It never shrinks to a token.
    if [[ $release == edge ]]; then
        check "$release: it lands at the size it was carried at" fact "$main" \
            --argjson w "$pane_w" --argjson h "$pane_h" \
            '.width >= ([$w, 1000] | min) - 2 and .height >= ([$h, 620] | min) - 2'
    fi
    check "$release: the other window stays visible" visible "$other"

    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .8
    report "$release-switched-off"
    check "$release: switching off leaves both visible" bash -c "$(declare -f fact visible probe); visible $main && visible $other"
    probe contactDrop
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: monitor bottom: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a monitor pane carried to the bottom edge and let go stays visible'
