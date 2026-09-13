#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: exit runtime line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
platform=${KADUNCE_EXIT_PLATFORM:-wayland}
output_role=${KADUNCE_TEST_OUTPUT_ROLE:-external}
[[ $output_role == external || $output_role == tablet ]]
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
touch_id=70
for kind in ${KADUNCE_EXIT_KINDS:-pointer touch}; do
for scenario in ${KADUNCE_EXIT_SCENARIOS:-detach multi withdrawn dock release}; do
    touch_id=$((touch_id + 2)); reentry_id=$((touch_id + 1))
    if [[ $platform == xcb ]]; then
        KADUNCE_TEST_FRAMELESS=1 QT_QPA_PLATFORM="$platform" \
            "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    else
        QT_QPA_PLATFORM="$platform" \
            "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    fi
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    if [[ $platform == xcb ]]; then sleep 1; else sleep .5; fi
    probe contactObserve
    if [[ $platform == xcb ]]; then
        # Xwayland has no xdg_toplevel move protocol. contactStart still selects
        # the client; its real _NET_WM_MOVERESIZE request supplies source proof.
        test "$(probe contactStart)" = false
    else
        test "$(probe contactStart)" = true
    fi
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
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down "$touch_id" "$x" "$y"; fi
    sleep .15
    if [[ $scenario == release ]]; then
        kad nativeCarryState | jq -e '(.carrying|not) and (.rendererActive|not) and (.inputBusy|not)'
    else
        kad nativeCarryState | jq -e '.carrying and .rendererActive and .inputBusy'
    fi
    y=799; if [[ $scenario == release ]]; then y=400; fi
    if [[ $kind == pointer ]]; then probe contactMotion 550 "$y"; else probe motion "$touch_id" 550 "$y"; fi
    if [[ $scenario != release ]]; then
        kad nativeCarryState | jq -e '.detachPreview and (.destinationRect.y + .destinationRect.height <= 790)'
    fi
    exit_target=$(kad nativeCarryState | jq -c '.destinationRect')
    if [[ $scenario == withdrawn || $scenario == dock ]]; then
        y=400; if [[ $scenario == dock ]]; then y=760; fi
        if [[ $kind == pointer ]]; then probe contactMotion 550 "$y"; else probe motion "$touch_id" 550 "$y"; fi
        kad nativeCarryState | jq -e '(.detachPreview|not)'
    fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up "$touch_id"; fi
    sleep .4
    echo "EXIT FIRST RELEASE $kind $scenario $(probe contactState)"
    if [[ $scenario == release ]]; then
        probe contactState | jq -e '(.moving|not)'
    fi
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
    fi
    if [[ $scenario == detach || $scenario == multi || $scenario == release ]]; then
        bounds=$(probe windowGeometry "$main")
        x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+.height/2|floor' <<<"$bounds")
        probe pointer "$x" "$y"; client armMove
        if [[ $kind == pointer ]]; then probe contactButton true; else probe down "$reentry_id" "$x" "$y"; fi
        sleep .1
        before_motion=$(probe windowGeometry "$main")
        if [[ $kind == pointer ]]; then probe contactMotion 700 400; else probe motion "$reentry_id" 700 400; fi
        after_motion=$(probe windowGeometry "$main")
        echo "EXIT NATIVE MOTION $kind $scenario before=$before_motion after=$after_motion"
        # A native move must follow the contact, not merely report inputBusy=false.
        # Stale native touch identity can swallow motion but still allow later
        # edge adoption, making a membership-only regression falsely pass.
        jq -en --argjson before "$before_motion" --argjson after "$after_motion" \
            '$before.x != $after.x or $before.y != $after.y'
        # A departed window is native even beside surviving Bento cards.
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not) and (.destinationPreview|not)'
        # The same fresh drag becomes an explicit Kadunce admission only when it
        # reaches an edge of the surviving Bento. Native KDE snapping must not win.
        echo "EXIT REENTRY before-edge $(probe contactState)"
        echo "EXIT ORDER before-edge $(probe edgeOrderState)"
        if [[ $kind == pointer ]]; then probe contactMotion 5 400; else probe motion "$reentry_id" 5 400; fi
        sleep .1
        echo "EXIT REENTRY at-edge $(probe contactState)"
        echo "EXIT ORDER at-edge $(probe edgeOrderState)"
        kad nativeMoveTrace
        kad nativeCarryState | jq -e '.carrying and .inputBusy and .destinationPreview and .placementOutline'
        if [[ $kind == pointer ]]; then probe contactButton false; else probe up "$reentry_id"; fi
        sleep .4
        mapfile -t stages < <(kad outputStageState)
        expected=1; if [[ $scenario == multi ]]; then expected=2; fi
        printf '%s\n' "${stages[@]}" | rg -q "^Virtual-0\\|${output_role}\\|.*\\|${expected}\\|0$"
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not) and (.destinationPreview|not)'
    fi
    test "$(probe releaseRuntime)" = true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    kill "$client_pid"; wait "$client_pid" || true
    sleep .2
done
done
echo "PASS: exit runtime platform=$platform kinds=${KADUNCE_EXIT_KINDS:-pointer touch} scenarios=${KADUNCE_EXIT_SCENARIOS:-detach multi withdrawn dock release}; native movement, release and selected exit/re-entry assertions"
