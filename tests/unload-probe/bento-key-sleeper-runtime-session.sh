#!/usr/bin/env bash
# CARD-LIFECYCLE.md §7 on the display that can own cards: a minimized card
# never silently returns to Bento. The Bento key pairs the Active card with
# the card used before it and leaves a sleeping card asleep, whether the
# sleeping card sits beside the pair or is itself the card used before the
# Active one, when the next awake card in Spread order is the partner.
#
# Needs the tablet fixture, alone: only a display that can own cards holds
# cards, and the Bento key goes to a monitor while one is attached.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: bento key sleeper: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId, bento: .desktopStage.active, apps: [.applications[] | {title, windowId, hasCard, stackSize, minimized}]}')"
}
# Meta+B by evdev code: KEY_B with KEY_LEFTMETA held.
bento_key() { probe key 48 125 >/dev/null; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for pass in beside before action; do
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    sleep 1
    probe contactObserve
    probe contactStart >/dev/null
    client colouredCompanion "Neighbour" "c03a3a" 700 500
    sleep .8
    neighbour=$(probe windowIdByCaption "Neighbour")
    client colouredCompanion "Sleeper" "2e8b57" 560 420
    sleep .8
    sleeper=$(probe windowIdByCaption "Sleeper")
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    main=$(kad workspaceContext | jq -r --arg n "$neighbour" --arg s "$sleeper" \
        '[.applications[] | select(.windowId != $n and .windowId != $s)][0].windowId')
    probe contactFocus >/dev/null
    sleep .5
    if [[ $pass != before ]]; then
        # The neighbour is the card used before the Active one; the sleeping
        # card sits beside them.
        probe minimizeWindow "$sleeper" true >/dev/null
        sleep .6
        probe activateWindowId "$neighbour" >/dev/null
        sleep .4
    else
        # The card used before the Active one is put to sleep, so it cannot
        # pair and the partner is the next awake card.
        probe activateWindowId "$neighbour" >/dev/null
        sleep .4
        probe activateWindowId "$sleeper" >/dev/null
        sleep .4
        probe contactFocus >/dev/null
        sleep .4
        probe minimizeWindow "$sleeper" true >/dev/null
        sleep .6
    fi
    probe contactFocus >/dev/null
    sleep .5
    report "$pass-before-key"
    check "$pass: the Active card" context --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
    check "$pass: the sleeping window is a card, asleep" \
        context --arg id "$sleeper" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .minimized'

    if [[ $pass == action ]]; then
        # The D-Bus Bento action composes the whole display rather than a
        # pair; what it does with a sleeping card is reported, not checked.
        kad toggleBentoOnOutput Virtual-0 >/dev/null
        sleep 1.5
        report "$pass-composed"
        echo "$pass: sleeping window after the action $(kad workspaceContext | jq -c --arg id "$sleeper" 'first(.applications[] | select(.windowId == $id)) | {hasCard, minimized, stackSize}')"
        test "$(probe releaseRuntime)" = true
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
        sleep .3
        probe contactDrop
        kill "$client_pid"; wait "$client_pid" 2>/dev/null
        sleep .3
        continue
    fi
    bento_key
    sleep 1.5
    report "$pass-paired"
    check "$pass: the key makes a Bento pair" context '.desktopStage.active'
    check "$pass: the Active card and the neighbour are its panes" \
        context --arg m "$main" --arg n "$neighbour" \
        '[.applications[] | select((.windowId == $m or .windowId == $n) and (.minimized | not))] | length == 2'
    check "$pass: the sleeping window is still a card, asleep" \
        context --arg id "$sleeper" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .minimized'
    check "$pass: no ownership violations" context '(.ownershipViolations // []) | length == 0'

    test "$(probe releaseRuntime)" = true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    probe contactDrop
    kill "$client_pid"; wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: bento key sleeper: $failures checks failed" >&2; exit 1; fi
echo 'PASS: the Bento key pairs two awake cards and leaves a sleeping card asleep'
