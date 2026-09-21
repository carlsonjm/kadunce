#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: tablet runtime line $LINENO" >&2' ERR
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
test "$(probe contactStart)" = "${KADUNCE_TEST_HAS_XDG:-true}"
client companion
sleep .2
test "$(probe contactFocus)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
# This session requires the disposable Virtual-0 tablet predicate fixture.
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
for scenario in open edge; do
for kind in pointer touch; do
    probe pointer 500 350
    kad showCardLine
    kad workspaceContext
    sleep .25
    kad showActive
    sleep .35
    state=$(kad workspaceContext); echo "$state"
    jq -e '.cardStage.presentation == "active"' <<<"$state"
    probe pointer 500 350
    client reset
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 47 500 350; fi
    sleep .15
    jq -e '.carrying and .inputBusy' <<<"$(kad nativeCarryState)"
    x=1800
    if [[ $scenario == edge ]]; then x=2555; fi
    if [[ $kind == pointer ]]; then probe contactMotion "$x" 380; else probe motion 47 "$x" 380; fi
    jq -e '.carrying and .destination' <<<"$(kad nativeCarryState)"
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 47; fi
    sleep .25
    jq -e '(.carrying|not) and (.inputBusy|not)' <<<"$(kad nativeCarryState)"
    if [[ $scenario == edge ]]; then
        kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-1" and .bentoActive)] | length == 1'
    else
        # Return as an ordinary monitor window, not a manually created Bento.
        kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-1" and .bentoActive)] | length == 0'
    fi
    sleep .25
    # Keep a resident on the tablet so return exercises Spread's receiver,
    # rather than only native arrival onto an empty/inactive tablet.
    kad showCardLine
    sleep .3
    kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"'
    probe pointer 1800 380
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 48 1800 380; fi
    sleep .15
    if [[ $scenario == open ]]; then
        jq -e '(.carrying|not) and (.inputBusy|not)' <<<"$(kad nativeCarryState)"
    else
        jq -e '.carrying and .inputBusy' <<<"$(kad nativeCarryState)"
    fi
    if [[ $kind == pointer ]]; then probe contactMotion 500 350; else probe motion 48 500 350; fi
    jq -e '.carrying and .destination' <<<"$(kad nativeCarryState)"
    if [[ $scenario == open ]]; then
        # Return to ordinary monitor space while still held: keep safe routing,
        # but no card-placement outline. Re-enter tablet to finish this case.
        if [[ $kind == pointer ]]; then probe contactMotion 1800 380; else probe motion 48 1800 380; fi
        kad nativeCarryState | jq -e '.carrying and .inputBusy and .destination and (.placementOutline|not)'
        if [[ $kind == pointer ]]; then probe contactButton false; else probe up 48; fi
        sleep .55
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
        kad workspaceContext | jq -e '[.displayContext.displays[]|select(.name=="Virtual-1" and .bentoActive)]|length==0'
        client armMove
        if [[ $kind == pointer ]]; then probe contactButton true; else probe down 48 1800 380; fi
        sleep .15
        kad nativeCarryState | jq -e '(.carrying|not) and (.inputBusy|not)'
        if [[ $kind == pointer ]]; then probe contactMotion 500 350; else probe motion 48 500 350; fi
        # Tablet arrival uses its own Spread presentation, not a Bento outline.
        kad nativeCarryState | jq -e '.carrying and .destination'
    fi
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 48; fi
    # It must start from the wide carried face, not teleport to the 64% center.
    kad nativeCarryState | jq -e '.lineAnimating and (.carrying|not)
        and (.inputBusy|not) and (.dropSettling|not) and .lineRect.width > 820'
    sleep .75
    jq -e '(.carrying|not) and (.inputBusy|not)' <<<"$(kad nativeCarryState)"
    state=$(kad workspaceContext); echo "$state"
    jq -e '.cardStage.presentation == "active"
        and ([.applications[] | select(.output == "Virtual-0")] | length == 2)' <<<"$state"
done
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: full Effect tablet Active pickup and ordinary/Bento-to-tablet return, pointer and touch (output predicate fixture)'
