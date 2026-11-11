#!/usr/bin/env bash
# An X11 application that asks for the same move twice on one press, as
# Steam's own title bar does: the Active card it carries follows the held
# button, stops at its release, and the pointer moving afterwards takes nothing
# with it.
set -euo pipefail
trap 'echo "FAIL: X11 repeated move line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
export QT_QPA_PLATFORM=xcb KADUNCE_TEST_FRAMELESS=1 KADUNCE_TEST_REPEAT_MOVE=1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
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
test "$(probe contactStart)" = false
client companion
sleep .2
test "$(probe contactFocus)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
for round in 1 2; do
    probe pointer 500 350
    kad showCardLine
    sleep .25
    kad showActive
    sleep .35
    jq -e '.cardStage.presentation == "active"' <<<"$(kad workspaceContext)"
    probe pointer 500 350
    client reset
    client armMove
    probe contactButton true
    # The repeated ask arrives while the button is still held.
    sleep .2
    jq -e '.carrying and .inputBusy' <<<"$(kad nativeCarryState)"
    probe contactMotion 700 450
    sleep .1
    # The held button still carries the card after the repeated ask.
    jq -e '.carrying and .inputBusy' <<<"$(kad nativeCarryState)"
    probe contactButton false
    sleep .4
    jq -e '(.carrying|not) and (.inputBusy|not)' <<<"$(kad nativeCarryState)"
    # Nothing follows the pointer once the button is up.
    main=$(kad workspaceContext | jq -r '.cardStage.activeCardId // empty')
    before=$(kad workspaceContext | jq -c '[.applications[] | .corners.frame]')
    probe contactMotion 300 200
    sleep .2
    probe contactMotion 900 600
    sleep .3
    after=$(kad workspaceContext | jq -c '[.applications[] | .corners.frame]')
    test "$before" = "$after"
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: an X11 move asked twice on one press is carried while held and ends at release'
