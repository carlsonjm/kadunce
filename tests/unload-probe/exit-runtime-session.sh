#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: exit runtime line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
for kind in pointer touch; do
for scenario in detach multi withdrawn dock release; do
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    sleep .5
    probe contactObserve
    test "$(probe contactStart)" = true
    probe entryClientsOnTablet
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    main=$(kad workspaceContext | jq -r '.applications[0].windowId')
    if [[ $scenario == multi ]]; then
        client companion
        sleep .2
        probe entryClientsOnTablet
        other=$(kad workspaceContext | jq -r --arg main "$main" '.applications[] | select(.windowId != $main) | .windowId')
        probe contactFocus
    fi
    test "$(kad toggleBentoOnOutput Virtual-0)" = true
    sleep .3
    if [[ $scenario == release ]]; then
        test "$(probe releaseRuntime)" = true
        sleep .3
        kad nativeCarryState | jq -e '(.rendererActive|not) and (.inputBusy|not)'
        kad workspaceContext | jq -e '(.desktopStage.active|not) and (.cardStage.active|not)'
    fi
    probe contactFocus
    bounds=$(probe windowGeometry "$main")
    x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+.height/2|floor' <<<"$bounds")
    probe pointer "$x" "$y"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 71 "$x" "$y"; fi
    sleep .15
    if [[ $scenario == release ]]; then
        kad nativeCarryState | jq -e '(.carrying|not) and (.rendererActive|not) and (.inputBusy|not)'
    else
        kad nativeCarryState | jq -e '.carrying and .rendererActive and .inputBusy'
    fi
    y=799; if [[ $scenario == release ]]; then y=400; fi
    if [[ $kind == pointer ]]; then probe contactMotion 550 "$y"; else probe motion 71 550 "$y"; fi
    if [[ $scenario != release ]]; then
        kad nativeCarryState | jq -e '.detachPreview and (.destinationRect.y + .destinationRect.height <= 790)'
    fi
    exit_target=$(kad nativeCarryState | jq -c '.destinationRect')
    if [[ $scenario == withdrawn || $scenario == dock ]]; then
        y=400; if [[ $scenario == dock ]]; then y=760; fi
        if [[ $kind == pointer ]]; then probe contactMotion 550 "$y"; else probe motion 71 550 "$y"; fi
        kad nativeCarryState | jq -e '(.detachPreview|not)'
    fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 71; fi
    sleep .4
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not) and (.detachPreview|not)'
    state=$(kad workspaceContext)
    if [[ $scenario == detach || $scenario == multi ]]; then
        probe windowGeometry "$main" | jq -e --argjson target "$exit_target" '. == $target'
    fi
    if [[ $scenario == detach || $scenario == release ]]; then
        jq -e '(.desktopStage.active|not)' <<<"$state"
        kad nativeCarryState | jq -e '(.rendererActive|not)'
    else jq -e '.desktopStage.active' <<<"$state"; fi
    if [[ $scenario == multi ]]; then
        probe windowGeometry "$other" | jq -e '.width > 1200 and .height > 700'
        bounds=$(probe windowGeometry "$main")
        x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+.height/2|floor' <<<"$bounds")
        probe pointer "$x" "$y"; client armMove
        if [[ $kind == pointer ]]; then probe contactButton true; else probe down 72 "$x" "$y"; fi
        sleep .1
        if [[ $kind == pointer ]]; then probe contactMotion 700 400; else probe motion 72 700 400; fi
        # A departed window is native even beside surviving Bento cards.
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not) and (.destinationPreview|not)'
        if [[ $kind == pointer ]]; then probe contactButton false; else probe up 72; fi
        sleep .4
        probe windowGeometry "$main" | jq -e --argjson before "$bounds" '. != $before'
        probe windowGeometry "$other" | jq -e '.width > 1200 and .height > 700'
    fi
    test "$(probe releaseRuntime)" = true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    kill "$client_pid"; wait "$client_pid" || true
    sleep .2
done
done
echo 'PASS: pointer/touch physical bottom-edge detach, withdrawal above edge and released-desktop render/input lifecycle'
