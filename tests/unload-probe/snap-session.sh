#!/usr/bin/env bash
set -euo pipefail
: "${KADUNCE_UNLOAD_PROBE_BUILD:?isolated probe build required}"
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
tr '\0' '\n' < "/proc/${PPID}/cmdline" | rg -q '^--virtual$'
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe >/dev/null 2>&1; then break; fi
    sleep .1
done
rg -Fq "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/kwin/effects/plugins/kadunce_unload_probe.so" "/proc/${PPID}/maps"
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for device in pointer touch; do
    for intercept in false true; do
        for target in left right top corner shift; do
            probe pointer 500 350
            test "$(probe snapStart "$intercept" "$([[ $device == touch ]] && echo true || echo false)")" = true
            sleep .1
            test "$(probe snapBeginMove)" = true
            sleep .1
            test "$(probe snapCapture)" = true
            if [[ $device == pointer ]]; then probe button true; else probe down 42 500 350; fi
            case $target in
                left) x=1; y=400 ;;
                right) x=1279; y=400 ;;
                top) x=640; y=1 ;;
                corner) x=1; y=799 ;;
                shift) x=100; y=400 ;;
            esac
            # Adopt before Shift: no native keyboard update may sneak in first.
            if [[ $device == pointer ]]; then probe pointer 510 360; else probe motion 42 510 360; fi
            if [[ $target == shift ]]; then probe shift true; fi
            if [[ $device == pointer ]]; then probe pointer "$x" "$y"; else probe motion 42 "$x" "$y"; fi
            sleep .3
            before=$(probe snapState)
            if [[ $device == pointer ]]; then probe button false; else probe up 42; fi
            sleep .1
            after=$(probe snapState)
            if [[ $target == shift ]]; then probe shift false; fi
            echo "$device $intercept $target BEFORE $before AFTER $after"
            if [[ $intercept == true ]]; then
                jq -e --argjson x "$x" --argjson y "$y" '
                    .adopted and (.moving|not) and (.outline|not) and
                    (.tiled|not) and (.maximized|not) and .outlineCount == 0 and
                    .steps == 0 and .finishes == 1 and .guardedFinishes == 1 and
                    .finishGuardCleared and .snapshotPreserved and .nativeUnmoved and
                    .poseX == ($x - .anchorX) and .poseY == ($y - .anchorY) and
                    .electricTiling and .electricMaximize' <<<"$before" >/dev/null
                jq -e '.released and (.tiled|not) and (.maximized|not) and (.moving|not)' <<<"$after" >/dev/null
            else
                jq -e '.outline and .outlineCount > 0 and .moving and .steps > 0' <<<"$before" >/dev/null
                jq -e '(.moving|not) and (.tiled or .maximized)' <<<"$after" >/dev/null
            fi
        done
    done
done
# Synchronous native finish is takeover, even when source cancels or recursively
# attempts another adoption inside the callback. Never resurrect after return.
for mode in cancel source output nested; do
    probe pointer 500 350
    test "$(probe snapStart true false)" = true
    probe snapInterrupt "$mode"
    sleep .1
    test "$(probe snapBeginMove)" = true
    sleep .1
    test "$(probe snapCapture)" = true
    probe button true
    probe pointer 510 360
    interrupted=$(probe snapState)
    echo "INTERRUPTION $mode $interrupted"
    jq -e '.adopted and (.moving|not) and .finishes == 1 and .guardedFinishes == 1
        and .finishGuardCleared and .snapshotPreserved and .nativeUnmoved' <<<"$interrupted" >/dev/null
    if [[ $mode == nested ]]; then
        jq -e '.busy and .nestedRejected' <<<"$interrupted" >/dev/null
        test "$(probe snapForeignOwnerRejected)" = true
    else
        jq -e '(.busy|not)' <<<"$interrupted" >/dev/null
    fi
    probe pointer 1279 400
    probe button false
    sleep .1
    jq -e '.released and (.moving|not) and (.tiled|not) and (.busy|not)
        and .outlineCount == 0 and .steps == 0' <<<"$(probe snapState)" >/dev/null
    expected=1
    if [[ $mode == source || $mode == output ]]; then expected=2; fi
    jq -e --argjson expected "$expected" '.resolution == $expected and
        .restoreToken == 1 and .revision == 1 and (.source|length)>0' <<<"$(probe snapOutcome)" >/dev/null
    test "$(probe snapOutcome)" = none
done
# A rejected source reservation must not cancel native movement or own its finish.
probe pointer 500 350
test "$(probe snapStart true false)" = true
probe snapInterrupt reject
sleep .1
test "$(probe snapBeginMove)" = true
sleep .1
test "$(probe snapCapture)" = true
probe button true
probe pointer 1279 400
sleep .3
jq -e '.moving and (.adopted|not) and .steps > 0 and .outline
    and .finishes == 0 and .guardedFinishes == 0' <<<"$(probe snapState)" >/dev/null
probe button false
sleep .1
jq -e '(.moving|not) and .tiled and .guardedFinishes == 0' <<<"$(probe snapState)" >/dev/null
# Keyboard updates must acquire ownership too, before the first motion event.
probe pointer 500 350
test "$(probe snapStart true false)" = true
sleep .1
test "$(probe snapBeginMove)" = true
sleep .1
test "$(probe snapCapture)" = true
probe button true
probe shift true
sleep .1
early=$(probe snapState)
echo "EARLY_SHIFT $early"
jq -e '(.moving|not) and .adopted and (.outline|not) and .outlineCount == 0 and .steps == 0' <<<"$early" >/dev/null
probe pointer 510 360
probe button false
probe shift false
probe snapDrop
# Removing the experiment must leave the next native drag entirely native.
probe pointer 500 350
test "$(probe snapStart false false)" = true
sleep .1
test "$(probe snapBeginMove)" = true
sleep .1
test "$(probe snapCapture)" = true
probe button true
probe pointer 1279 400
sleep .3
probe button false
sleep .1
fresh=$(probe snapState)
echo "FRESH_NATIVE $fresh"
jq -e '.tiled and (.moving|not) and .electricTiling and .electricMaximize' <<<"$fresh" >/dev/null
probe snapDrop
echo 'PASS: private native snap controls and pre-step pointer/touch carry adoption (model pose only)'
echo 'PASS: early-Shift also adopts before native preview (keyboard entry covered)'
echo 'PASS: subsequent native drag still tiles after removing the experiment'
echo 'PASS: native takeover preserves observed snapshot, guards synchronous finish, rejects foreign owners and drains interrupted/reentrant attempts'
echo 'PASS: output-loss notification invalidates carry; exact source outcomes are one-shot; rejected reservations stay native'
