#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: contact probe line $LINENO" >&2' ERR
: "${KADUNCE_UNLOAD_PROBE_BUILD:?isolated probe build required}"
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
tr '\0' '\n' < "/proc/${PPID}/cmdline" | rg -q '^--virtual$'
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
# KWin's maps are unreadable under a restricted-ptrace kernel, so ask KWin
# whether the probe loaded instead of reading its address space.
for attempt in {1..40}; do
    if [[ $(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe 2>/dev/null) == true ]]; then break; fi
    sleep .1
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded kadunce_unload_probe | rg -qx true
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
for kind in pointer touch; do
    # Re-create observer after the client exists: no shell creation subscription.
    probe contactDrop
    probe contactObserve
    test "$(probe contactStart)" = true
    probe pointer 500 350
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 42 500 350; fi
    sleep .2
    evidence=$(probe contactState)
    echo "SERIAL $kind $evidence"
    jq -e --arg kind "$kind" '.starts == 1 and .requests == 1 and .matches == 1
        and .kind == $kind and .startedBeforeRequest and .moving' <<<"$evidence"
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 42; fi
    probe contactEndMove
    jq -e '(.candidate|not)' <<<"$(probe contactState)"
done
# Actual source controller + correlated app request + guarded native cancellation.
for source in false true; do
    for mode in accept reject interrupt; do
        for kind in pointer touch; do
            test "$(probe contactStart)" = true
            test "$(probe handoffArm "$source" "$([[ $mode == reject ]] && echo true || echo false)" "$([[ $mode == interrupt ]] && echo true || echo false)")" = true
            sleep .15
            probe pointer 500 350
            client reset
            client armMove
            if [[ $kind == pointer ]]; then probe contactButton true; else probe down 47 500 350; fi
            sleep .1
            handoff=$(probe handoffState)
            beforeRelease=$(client state)
            echo "HANDOFF $source $kind $mode $handoff"
            echo "CLIENT AFTER TAKEOVER $beforeRelease"
            if [[ $mode == reject ]]; then
                jq -e '.result == 0 and .fallbacks == 1 and .finishes == 0 and .moving and .sourceValid' <<<"$handoff"
            elif [[ $mode == interrupt ]]; then
                jq -e '.result == 2 and .fallbacks == 0 and .finishes == 1 and (.moving|not) and (.busy|not) and .sourceValid' <<<"$handoff"
            else
                jq -e '.result == 1 and .fallbacks == 0 and .finishes == 1 and (.moving|not) and .busy and .sourceValid and .retained' <<<"$handoff"
            fi
            if [[ $mode == accept ]]; then
                if [[ $kind == pointer ]]; then probe contactMotion 1250 380; else probe motion 47 1250 380; fi
                sleep .15
                paint=$(probe handoffState)
                echo "CARRY PAINT $source $kind $paint"
                jq -e '.moves == 1 and .inputBusy and .paintCalls > 0 and .paintOutputs == 2
                    and .paintGeometryChanges == 0' <<<"$paint"
            fi
            if [[ $kind == pointer ]]; then probe contactButton false; else probe up 47; fi
            ended=$(probe handoffState)
            echo "RELEASE $source $kind $mode $ended"
            if [[ $mode == accept ]]; then
                jq -e '.releases == 1 and (.inputBusy|not) and (.busy|not)' <<<"$ended"
            else
                jq -e '.releases == 0 and (.inputBusy|not)' <<<"$ended"
            fi
            if [[ $mode != reject ]]; then
                sleep .05
                afterRelease=$(client state)
                echo "CLIENT AFTER PHYSICAL RELEASE $afterRelease"
                jq -e --argjson before "$beforeRelease" '.release == $before.release and .touchUp == $before.touchUp' <<<"$afterRelease"
            fi
            test "$(probe handoffDisarm)" = true
            sleep .15
        done
    done
done
# Cancel an established carry; the remaining physical release cannot become a drop.
for kind in pointer touch; do
    test "$(probe contactStart)" = true
    test "$(probe handoffArm false false false)" = true
    sleep .15
    probe pointer 500 350
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 47 500 350; fi
    sleep .1
    probe handoffCancelInput
    jq -e '.routeCancels == 1 and .inputBusy and (.busy|not)' <<<"$(probe handoffState)"
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 47; fi
    jq -e '.releases == 0 and (.inputBusy|not)' <<<"$(probe handoffState)"
    test "$(probe handoffDisarm)" = true
done
for canceled in false true; do
    test "$(probe contactStart)" = true
    test "$(probe handoffArm false false false)" = true
    probe handoffUnidentified "$canceled"
    sleep .1
    pending=$(probe handoffState)
    echo "UNIDENTIFIED $canceled $pending"
    expected=1
    if [[ $canceled == true ]]; then expected=0; fi
    jq -e --argjson expected "$expected" '.fallbacks == $expected and .result == -1
        and .finishes == 0 and .moving and .sourceValid' <<<"$pending"
    test "$(probe handoffDisarm)" = true
done
# A real request during mixed input remains ambiguous, even with a valid serial.
test "$(probe contactStart)" = true
probe pointer 500 350
probe contactButton true
client armMove
probe down 43 500 350
sleep .2
mixed=$(probe contactState)
echo "MIXED $mixed"
jq -e '.requests == 1 and .matches == 0 and (.candidate|not)' <<<"$mixed"
probe up 43
probe contactEndMove
probe contactButton false
# A move operation without a client press request must not borrow a held contact.
test "$(probe contactStart)" = true
probe contactButton true
probe contactKeyboardMove
jq -e '.starts == 1 and .requests == 0 and .matches == 0 and .moving' <<<"$(probe contactState)"
probe contactEndMove
probe contactButton false
# Cancel removes touch even though KWin drops the subsequent up; pointer survives.
test "$(probe contactStart)" = true
probe contactButton true
probe down 9 500 350
jq -e '(.candidate|not)' <<<"$(probe contactState)"
probe contactCancel
jq -e '.cancel == 1 and .candidate' <<<"$(probe contactState)"
probe up 9
probe contactButton false
jq -e '(.candidate|not)' <<<"$(probe contactState)"
probe down 9 500 350
jq -e '.candidate' <<<"$(probe contactState)"
probe contactCancel
jq -e '.cancel == 2 and (.candidate|not)' <<<"$(probe contactState)"
probe contactButton true
probe contactRemoveDevice
jq -e '(.candidate|not)' <<<"$(probe contactState)"
probe contactAddDevice
probe contactButton false
# Touch IDs have no device in KWin's spy API: removal still clears the seat's
# reservation even when that device has never supplied a pointer down.
probe down 19 500 350
jq -e '.candidate' <<<"$(probe contactState)"
probe contactRemoveDevice
jq -e '(.candidate|not)' <<<"$(probe contactState)"
probe contactAddDevice
probe contactCancel
probe contactDrop
# System decoration drag: use the actual titlebar, not a direct move operation.
for kind in pointer touch; do
    probe contactObserve
    test "$(probe contactStart)" = true
    test "$(probe contactPrepareDecoration)" = true
    sleep .5
    bounds=$(probe contactState)
    echo "DECORATION BOUNDS $bounds"
    jq -e '.titleHeight > 0' <<<"$bounds"
    x=$(jq '.titleX | floor' <<<"$bounds")
    y=$(jq '.titleY | floor' <<<"$bounds")
    probe pointer "$x" "$y"
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 44 "$x" "$y"; fi
    if [[ $kind == pointer ]]; then probe pointer "$((x+50))" "$((y+50))"; else probe motion 44 "$((x+50))" "$((y+50))"; fi
    sleep .1
    decorated=$(probe contactState)
    echo "DECORATION $kind $decorated"
    jq -e --arg kind "$kind" '.starts == 1 and .decorationMatches == 1
        and .requests == 0 and .kind == $kind and .moving' <<<"$decorated"
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 44; fi
    probe contactEndMove
    probe contactDrop
done
# Input remains usable after observer destruction.
client reset
probe pointer 500 350
probe contactButton true
probe contactButton false
sleep .1
jq -e '.press == 1 and .release == 1' <<<"$(client state)"
probe contactObserve
test "$(probe contactStart)" = true
client companion
sleep .2
test "$(probe contactLookupAll)" = true
# The Active and Bento destination routes that followed here needed a display
# able to own cards, which no display this harness creates is; they wait on
# physical review (CURRENT_STATE.md § Validation boundary).
client closeWindow
wait "$client_pid"
# The compositor retires the client's resources on its own schedule, which a
# loaded machine stretches; wait for it rather than for a fixed time.
for attempt in {1..30}; do
    if jq -e '(.protocolAlive|not) and (.clientAlive|not)' <<<"$(probe contactState)" >/dev/null; then break; fi
    sleep .1
done
jq -e '(.protocolAlive|not) and (.clientAlive|not)' <<<"$(probe contactState)"
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
sleep 1
test "$(probe contactStart)" = true
jq -e '.protocolAlive and .clientAlive' <<<"$(probe contactState)"
probe contactDrop
echo 'PASS: real client move serials correlate pointer and seat-scoped touch; native start precedes request observer'
echo 'PASS: keyboard move operation cannot borrow held pointer; cancellation and device removal clear candidates'
echo 'PASS: observer unload leaves native input routing intact (no takeover enabled)'
echo 'PASS: system titlebar pointer/touch motion correlates during decoration event dispatch'
echo 'PASS: exported resource lookup handles existing clients, observer recreation, close and new client'
