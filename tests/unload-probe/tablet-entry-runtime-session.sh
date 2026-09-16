#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: tablet entry line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
test "$(probe contactStart)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
main=$(kad workspaceContext | jq -r '.applications[0].windowId')
original=$(probe windowGeometry "$main")
# The same shortcut destination must work with an external monitor attached.
test "$(kad toggleBentoOnOutput Virtual-0)" = true
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name=="Virtual-0" and .bentoActive)] | length==1'
test "$(kad toggleBentoOnOutput Virtual-0)" = true
for stage in line active; do
for kind in pointer touch; do
for scenario in edge withdrawn; do
    probe pointer 500 350
    kad showCardLine
    sleep .35
    if [[ $stage == active ]]; then kad showActive; sleep .3; client armMove; fi
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 61 500 350; fi
    sleep .4
    kad nativeCarryState | jq -e '.carrying or .lineCarrying'
    if [[ $kind == pointer ]]; then probe contactMotion 5 350; else probe motion 61 5 350; fi
    kad nativeCarryState | jq -e '.destinationPreview'
    if [[ $scenario == withdrawn ]]; then
        if [[ $kind == pointer ]]; then probe contactMotion 500 500; else probe motion 61 500 500; fi
        kad nativeCarryState | jq -e '(.destinationPreview|not)'
    fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 61; fi
    sleep .55
    kad nativeCarryState | jq -e '(.carrying|not) and (.lineCarrying|not) and (.inputBusy|not)'
    count=$(kad workspaceContext | jq '[.displayContext.displays[] | select(.name=="Virtual-0" and .bentoActive)] | length')
    if [[ $scenario == edge ]]; then
        test "$count" = 1
        probe windowGeometry "$main" | jq -e '.width > 1200 and .height > 700'
        test "$(kad toggleBentoOnOutput Virtual-0)" = true
    else test "$count" = 0; fi
done
done
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
probe windowGeometry "$main" | jq -e --argjson original "$original" '. == $original'
echo 'PASS: tablet Bento shortcut destination, Active/Card Line edge entry and withdrawal with pointer/touch; lone card and restoration'
for origin in ordinary bento; do
    # Precommit cancellation restores monitor pickup; committed tablet ownership
    # releases on tablet. Use fresh clients so each case starts independently.
    kill "$client_pid"
    wait "$client_pid" || true
    probe pointer 500 350
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .5
    test "$(probe contactStart)" = true
    client companion
    sleep .3
    test "$(probe entryClientsOnTablet)" = true
    sleep .3
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    main=$(kad workspaceContext | jq -r '.applications[0].windowId')
    probe contactFocus
    test "$(kad toggleBentoOnOutput Virtual-0)" = true
    test "$(kad handoffBentoLeadToOutput Virtual-0 Virtual-1)" = true
    if [[ $origin == bento ]]; then test "$(kad toggleBentoOnOutput Virtual-1)" = true; fi
    sleep .3
    bounds=$(probe windowGeometry "$main")
    x=$(jq '.x + .width/2 | floor' <<<"$bounds"); y=$(jq '.y + .height/2 | floor' <<<"$bounds")
    probe pointer "$x" "$y"
    client armMove
    probe contactButton true
    sleep .1
    if [[ $origin == ordinary ]]; then
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
    else
        kad nativeCarryState | jq -e '.carrying'
    fi
    probe contactMotion 500 350
    kad nativeCarryState | jq -e '.destinationPreview'
    probe contactButton false
    sleep .3
    kad workspaceContext | jq -e '.cardStage.active == false
        and ([.displayContext.displays[] | select(.name=="Virtual-0" and .bentoActive)]|length==1)
        and ([.applications[] | select(.output=="Virtual-0")]|length==2)'
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
done
echo 'PASS: existing tablet Bento receives ordinary and Bento native carries without re-entering Card Line'
