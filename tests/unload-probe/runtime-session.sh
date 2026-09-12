#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: runtime line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
test "$(probe contactStart)" = true
for scenario in open edge withdrawn; do
for kind in pointer touch; do
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    test "$(kad toggleBentoOnOutput Virtual-0)" = true
    sleep .3
    probe pointer 500 350
    client reset
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 47 500 350; fi
    sleep .15
    state=$(kad nativeCarryState); echo "RUNTIME PICKUP $kind $state"
    jq -e '.carrying and .inputBusy' <<<"$state"
    x=1800
    if [[ $scenario != open ]]; then x=2555; fi
    if [[ $kind == pointer ]]; then probe contactMotion "$x" 380; else probe motion 47 "$x" 380; fi
    sleep .1
    jq -e '.destination and .carrying and .destinationPreview' <<<"$(kad nativeCarryState)"
    if [[ $scenario == withdrawn ]]; then
        if [[ $kind == pointer ]]; then probe contactMotion 1800 380; else probe motion 47 1800 380; fi
    fi
    destination=$(kad nativeCarryState | jq -c '.destinationRect')
    window_id=$(kad workspaceContext | jq -r '.applications[0].windowId')
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 47; fi
    if [[ $scenario == edge ]]; then
        kad nativeCarryState | jq -e '.dropSettling and (.carrying|not) and (.inputBusy|not)'
    fi
    sleep .2
    jq -e '(.carrying|not) and (.inputBusy|not) and (.destinationPreview|not)' <<<"$(kad nativeCarryState)"
    probe windowGeometry "$window_id" | jq -e --argjson target "$destination" '. == $target'
    context=$(kad workspaceContext)
    if [[ $scenario == edge ]]; then
        jq -e '[.displayContext.displays[] | select(.name == "Virtual-1" and .bentoActive)] | length == 1' <<<"$context"
    else
        jq -e '[.displayContext.displays[] | select(.name == "Virtual-1" and .bentoActive)] | length == 0' <<<"$context"
        test "$(kad toggleBentoOnOutput Virtual-1)" = true
    fi
    test "$(kad handoffBentoLeadToOutput Virtual-1 Virtual-0)" = true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .2
done
done
for kind in pointer touch; do
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    test "$(kad toggleBentoOnOutput Virtual-0)" = true
    sleep .3
    probe pointer 500 350
    client reset
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 47 500 350; fi
    sleep .1
    jq -e '.carrying and .inputBusy' <<<"$(kad nativeCarryState)"
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    before=$(client state)
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 47; fi
    sleep .1
    after=$(client state)
    echo "RUNTIME UNLOAD $kind $before $after"
    jq -e --argjson before "$before" '.release == $before.release and .touchUp == $before.touchUp' <<<"$after"
done
echo 'PASS: production Effect pointer/touch adoption, carry rendering and monitor release'
echo 'PASS: production Effect unload during held pointer/touch is immediate and remaining release is inert'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .3
probe pointer 500 350
client armMove
probe contactButton true
sleep .15
probe contactMotion 2555 380
probe contactButton false
kad nativeCarryState | jq -e '.dropSettling and (.inputBusy|not)'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad nativeCarryState | jq -e '(.dropSettling|not) and (.inputBusy|not)'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: unload during committed monitor settle has no surviving animation/input'
