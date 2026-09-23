#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: desktop runtime line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for kind in pointer touch; do
for scenario in edge edge-refresh edge-refresh-reject external withdrawn unload dock; do
    echo "DESKTOP $kind $scenario"
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    sleep .5
    probe contactObserve
    test "$(probe contactStart)" = true
    # The only window yet, before Kadunce places anything.
    original=$(probe frames | jq -c '[.[]][0]')
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .2
    main=$(kad workspaceContext | jq -r '.applications[0].windowId')
    tablet=$(kad workspaceContext | jq '[.displayContext.displays[]|select(.name=="Virtual-0" and .role=="tablet")]|length')
    if [[ $tablet == 1 ]]; then
        # CARD-LIFECYCLE.md §3: switching Kadunce on puts the display that can
        # own cards in cards, the window in use Active.
        kad workspaceContext | jq -e --arg main "$main" '.cardStage.active
            and .cardStage.presentation == "active" and .cardStage.selectedCardId == $main'
    fi
    client companion
    sleep .2
    other=$(kad workspaceContext | jq -r --arg main "$main" '.applications[] | select(.windowId != $main) | .windowId')
    original_other=$(probe windowGeometry "$other")
    probe contactFocus
    probe pointer 500 350
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 52 500 350; fi
    sleep .1
    # A desktop window is carried only once it reaches an edge; a card is
    # carried from the start.
    if [[ $tablet == 0 ]]; then kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'; fi
    x=1275; if [[ $scenario == external ]]; then x=2555; fi
    if [[ $kind == pointer ]]; then probe contactMotion "$x" 380; else probe motion 52 "$x" 380; fi
    kad nativeCarryState | jq -e '.carrying and .inputBusy and .destinationPreview'
    if [[ $scenario == edge-refresh ]]; then
        # A client can publish a still-feasible size hint after edge preview.
        # The release-time solve must use that current hint without requiring a
        # second snap, while the resident/output reservation stays unchanged.
        client minimumSizeHint 300 240
        sleep .1
    fi
    if [[ $scenario == edge-refresh-reject ]]; then
        client minimumSizeHint 2000 1200
        sleep .1
    fi
    if [[ $scenario == withdrawn || $scenario == dock ]]; then
        y=400; if [[ $scenario == dock ]]; then y=780; fi
        if [[ $kind == pointer ]]; then probe contactMotion 550 "$y"; else probe motion 52 550 "$y"; fi
    fi
    destination=$(kad nativeCarryState | jq -c '.destinationRect')
    if [[ $scenario == dock ]]; then
        # The bottom band belongs to the dock on every display, so nothing is
        # placed over it. A desktop window is offered a destination beside it;
        # a card carried there is offered none and stays the card it was.
        if [[ $tablet == 0 ]]; then kad nativeCarryState | jq -e '.destination and (.placementOutline|not)'
        else kad nativeCarryState | jq -e '(.destination|not) and (.placementOutline|not)'; fi
    fi
    if [[ $scenario == unload ]]; then
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 52; fi
    sleep .25
    if [[ $scenario != unload ]]; then
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
        active=$(kad workspaceContext | jq '[.displayContext.displays[] | select(.bentoActive)] | length')
        if [[ $scenario == edge || $scenario == edge-refresh || $scenario == edge-refresh-reject || $scenario == external ]]; then kad nativeMoveTrace; fi
        # A display that cannot own cards composes Bento across itself. On the
        # tablet both windows are already cards, so a side release pairs the
        # carried card with the Active one (§3). The external scenario releases
        # on the other output in both fixtures.
        if [[ $scenario == edge || $scenario == edge-refresh || $scenario == external ]]; then test "$active" = 1
        else test "$active" = 0; fi
        if [[ $scenario == edge || $scenario == edge-refresh ]]; then
            kad outputStageState | rg '^Virtual-0\|.*\|2$'
        fi
        if [[ $scenario == edge || $scenario == edge-refresh || $scenario == external
              || ( $tablet == 0 && ( $scenario == withdrawn || $scenario == dock ) ) ]]; then
            probe windowGeometry "$main" | jq -e --argjson target "$destination" '. == $target'
        fi
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    fi
    sleep .2
    if [[ $scenario != withdrawn && $scenario != dock && $scenario != edge-refresh-reject ]]; then
        probe windowGeometry "$main" | jq -e --argjson target "$original" '. == $target'
    fi
    if [[ $scenario != edge-refresh-reject && $tablet == 0 ]]; then
        probe windowGeometry "$other" | jq -e --argjson target "$original_other" '. == $target'
    elif [[ $scenario != edge-refresh-reject ]]; then
        # The companion opened as a card; release returns the size it asked for.
        probe windowGeometry "$other" | jq -e '.width == 500 and .height == 400'
    fi
    kill "$client_pid"
    wait "$client_pid" || true
    sleep .1
done
done
echo 'PASS: ordinary desktop pointer/touch edge-created Bento, withdrawn preview, dock exclusion and held unload/restoration'
if [[ $tablet == 1 ]]; then echo 'PASS: switching Kadunce on puts the tablet in cards, the window in use Active'; fi
