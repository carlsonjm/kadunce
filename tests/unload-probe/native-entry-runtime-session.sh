#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: native entry line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
QT_QPA_PLATFORM=${KADUNCE_ENTRY_PLATFORM:-wayland} "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
probe contactStart
for scenario in ${KADUNCE_ENTRY_SCENARIOS:-open local cross disabled bottom bottom-cancel cross-open}; do
for kind in pointer touch; do
    probe contactPrepareDecoration
    sleep .2
    edge_preferences=$(probe edgeOptions)
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    test "$(probe edgeOptions)" = '0|0'
    probe reloadEdgeOptions
    test "$(probe edgeOptions)" = '0|0'
    main=$(kad workspaceContext | jq -r '.applications[0].windowId')
    original=$(probe windowGeometry "$main")
    probe pointer 500 350
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 62 500 350; fi
    sleep .15
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not) and (.destinationPreview|not)'
    if [[ $kind == pointer ]]; then probe contactMotion 650 400; else probe motion 62 650 400; fi
    sleep .15
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not) and (.destinationPreview|not)'
    probe windowGeometry "$main" | jq -e --argjson original "$original" '. != $original'
    if [[ $scenario == cross-open ]]; then
        # Acquire at a source edge, then continue (without releasing) to a free
        # second display. This is not a return to the source display.
        if [[ $kind == pointer ]]; then probe contactMotion 5 400; else probe motion 62 5 400; fi
        kad nativeCarryState | jq -e '.carrying and .destination'
        if [[ $kind == pointer ]]; then probe contactMotion 1800 400; else probe motion 62 1800 400; fi
        kad nativeCarryState | jq -e '.carrying and .destination and (.placementOutline|not)'
    fi
    if [[ $scenario == bottom || $scenario == bottom-cancel ]]; then
        if [[ $kind == pointer ]]; then probe contactMotion 700 799; else probe motion 62 700 799; fi
        sleep .15
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
        if [[ $scenario == bottom-cancel ]]; then probe contactEndMove; fi
    fi
    if [[ $scenario == cross ]]; then
        if [[ $kind == pointer ]]; then probe contactMotion 1800 400; else probe motion 62 1800 400; fi
        sleep .15
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
    fi
    if [[ $scenario == local || $scenario == cross ]]; then
        edge=5; output=Virtual-0
        if [[ $scenario == cross ]]; then edge=2555; output=Virtual-1; fi
        if [[ $kind == pointer ]]; then probe contactMotion "$edge" 400; else probe motion 62 "$edge" 400; fi
        sleep .15
        kad nativeMoveTrace
        kad nativeCarryState | jq -e '.carrying and .inputBusy and .destinationPreview'
    fi
    if [[ $scenario == disabled ]]; then
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 62; fi
    sleep .2
    if [[ $scenario == disabled ]]; then
        test "$(probe edgeOptions)" = "$edge_preferences"
        continue
    fi
    if [[ $scenario == cross-open ]]; then
        kad workspaceContext | jq -e --arg main "$main" '[.applications[]|select(.windowId==$main and .output=="Virtual-1")]|length==1'
    fi
    if [[ $scenario == bottom ]]; then
        probe windowGeometry "$main" | jq -e --argjson original "$original" '.y + .height <= 790 and .y >= 10 and .width == $original.width and .height == $original.height'
    elif [[ $scenario == bottom-cancel ]]; then
        probe windowGeometry "$main" | jq -e --argjson original "$original" '. == $original'
    fi
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
    if [[ $scenario == local || $scenario == cross ]]; then
        kad workspaceContext | jq -e --arg output "$output" '[.displayContext.displays[]|select(.name==$output and .bentoActive)]|length==1'
    else
        kad workspaceContext | jq -e '[.displayContext.displays[]|select(.bentoActive)]|length==0'
    fi
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    test "$(probe edgeOptions)" = "$edge_preferences"
    sleep .2
done
done
echo "PASS: native entry scenarios=${KADUNCE_ENTRY_SCENARIOS:-open local cross disabled bottom bottom-cancel cross-open}; pointer/touch"
