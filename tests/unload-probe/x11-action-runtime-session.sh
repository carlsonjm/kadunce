#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: X11 action check line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe
KADUNCE_TEST_FRAMELESS=1 QT_QPA_PLATFORM=xcb "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
test "$(probe contactStart)" = false
client clickTarget
sleep .3
for mode in native kadunce; do
for kind in pointer touch; do
    if [[ $mode == kadunce ]]; then
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
        # Exercise cancellation after ownership, not a deferred ordinary move.
        qdbus6 org.kde.KWin /Kadunce toggleBentoOnOutput Virtual-0
        sleep .3
    fi
    probe contactFocus
    probe pointer 500 350
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 62 500 350; fi
    sleep .2
    if [[ $mode == kadunce ]]; then
        qdbus6 org.kde.KWin /Kadunce nativeCarryState | jq -e '.carrying and .inputBusy'
    fi
    if [[ $kind == pointer ]]; then probe contactMotion 1650 350; else probe motion 62 1650 350; fi
    if [[ $mode == kadunce ]]; then
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    else
        probe contactEndMove
    fi
    sleep .2
    before=$(client targetState)
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 62; fi
    sleep .2
    after=$(client targetState)
    echo "ACTION CHECK $mode $kind $before $after"
    jq -e --argjson before "$before" '.==$before' <<<"$after"
    # Positive control: an intentional new click must still activate the button.
    probe pointer 1650 350
    probe contactButton true
    probe contactButton false
    sleep .2
    client targetState | jq -e --argjson before "$before" '.click==$before.click+1'
done
done
echo 'PASS: native and Kadunce pointer/touch cancellation do not activate the target; fresh clicks work'
