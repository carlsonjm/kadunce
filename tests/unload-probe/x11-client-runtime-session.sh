#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: X11 client entry line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
KADUNCE_TEST_FRAMELESS=1 QT_QPA_PLATFORM=xcb "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
test "$(probe contactStart)" = false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
# No held contact, wrong button, and resize must not become carries.
client x11Request 8 1
sleep .1
kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
for args in '8 3' '4 1'; do
    probe pointer 500 350
    probe contactButton true
    client x11Request $args
    sleep .1
    kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
    client x11Request 11 0
    probe contactButton false
done
probe pointer 1800 350
probe contactButton true
client x11Request 8 1
sleep .1
kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
client x11Request 11 0
probe contactButton false
for source in ordinary card; do
for kind in pointer touch; do
    main=$(kad workspaceContext | jq -r '.applications[0].windowId')
    bounds=$(probe windowGeometry "$main")
    if [[ $source == card ]]; then
        output=Virtual-0
        if jq -e '.x>=1280' <<<"$bounds" >/dev/null; then output=Virtual-1; fi
        test "$(kad toggleBentoOnOutput "$output")" = true
        sleep .2
        bounds=$(probe windowGeometry "$main")
    fi
    x=$(jq '(.x+.width/2)|floor' <<<"$bounds"); y=$(jq '(.y+.height/2)|floor' <<<"$bounds")
    probe pointer "$x" "$y"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 62 "$x" "$y"; fi
    sleep .2
    kad nativeMoveTrace
    if [[ $source == ordinary ]]; then
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
    else
        kad nativeCarryState | jq -e '.carrying and .inputBusy'
    fi
    kad nativeMoveTrace | grep "proof-xwayland-$kind" >/dev/null
    target=Virtual-1; edge=2555
    if ((x>=1280)); then target=Virtual-0; edge=5; fi
    if [[ $kind == pointer ]]; then probe contactMotion "$edge" 350; else probe motion 62 "$edge" 350; fi
    kad nativeCarryState | jq -e '.carrying and .inputBusy and .destinationPreview'
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 62; fi
    sleep .3
    kad workspaceContext | jq -e --arg target "$target" '[.displayContext.displays[] | select(.name==$target and .bentoActive)]|length==1'
    test "$(kad toggleBentoOnOutput "$target")" = true
    sleep .3
done
done
for kind in pointer touch; do
    test "$(kad toggleBentoOnOutput Virtual-0)" = true
    sleep .3
    main=$(kad workspaceContext | jq -r '.applications[0].windowId')
    bounds=$(probe windowGeometry "$main")
    x=$(jq '(.x+.width/2)|floor' <<<"$bounds"); y=$(jq '(.y+.height/2)|floor' <<<"$bounds")
    probe pointer "$x" "$y"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 62 "$x" "$y"; fi
    sleep .15
    kad nativeCarryState | jq -e '.carrying and .inputBusy'
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    # Xwayland can synthesize a matching release on pointer re-entry even after
    # native-only KWin cancellation. Accept that bounded cleanup, never a new
    # press. x11-action-runtime-session separately checks real button activation
    # and fresh-click recovery. User approved native-equivalent behavior.
    sleep .2
    before=$(client state)
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 62; fi
    sleep .1
    after=$(client state)
    echo "X11 UNLOAD $kind $before $after"
    jq -e --argjson before "$before" '
        .press==$before.press and .touchDown==$before.touchDown
        and .release>=$before.release and .release<=$before.release+1 and .release<=.press
        and .touchUp>=$before.touchUp and .touchUp<=$before.touchUp+1 and .touchUp<=.touchDown
    ' <<<"$after"
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: Xwayland client-request pointer/touch external edge entry'
echo 'PASS: Xwayland no-contact, wrong-button and resize requests remain native'
echo 'PASS: Xwayland wrong-surface rejection and held pointer/touch unload'
