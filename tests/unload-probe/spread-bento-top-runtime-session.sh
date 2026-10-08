#!/usr/bin/env bash
# INPUT.md § Active card and CARD-LIFECYCLE.md §6 on the display that can own
# cards: a Bento pair chosen in Spread opens as its layout, and a pane of it
# dragged by its title bar to the top edge leaves the layout as the Active
# card. A layout resumed from Spread is a placed one, so its panes carry as
# they do when the layout is first made: with no other card, beside a sleeping
# card, and once that sleeping card has closed, which leaves the layout as it
# was. At a fractional scale the pair holds though a client's frame lands
# part of a pixel off its pane.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: spread bento top: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '{p: .cardStage.presentation, active: .cardStage.active, sel: .cardStage.selectedCardId, bento: .desktopStage.active, apps: [.applications[] | {title, windowId, hasCard, stackSize, selected, minimized}]}')"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
touch=90
for pass in alone sleeper closed; do
    touch=$((touch + 3))
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
    sleeper=
    if [[ $pass != alone ]]; then
        client colouredCompanion "Sleeper" "2e8b57" 560 420
        sleep .8
        sleeper=$(probe windowIdByCaption "Sleeper")
    fi
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    main=$(kad workspaceContext | jq -r --arg n "$neighbour" --arg s "$sleeper" \
        '[.applications[] | select(.windowId != $n and .windowId != $s)][0].windowId')
    probe contactFocus >/dev/null
    sleep .5
    if [[ -n $sleeper ]]; then
        # A card put to sleep beside the pair, as a minimized application is.
        probe minimizeWindow "$sleeper" true >/dev/null
        sleep .6
    fi
    # The neighbour is the card used before the Active one, so it is the partner.
    probe activateWindowId "$neighbour" >/dev/null
    sleep .4
    probe contactFocus >/dev/null
    sleep .5
    check "$pass: the Active card" context --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'

    # Carried by its title bar to the left edge, the Active card pairs with
    # its neighbour, the card used before it; then Spread.
    bounds=$(probe windowGeometry "$main")
    x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+40|floor' <<<"$bounds")
    probe pointer "$x" "$y"
    client armMove
    probe down "$((touch + 2))" "$x" "$y"
    sleep .15
    probe motion "$((touch + 2))" 650 400; sleep .15
    probe motion "$((touch + 2))" 5 400; sleep .3
    probe up "$((touch + 2))"
    sleep 1.2
    report "$pass-paired"
    # At a fractional scale a pane can sit part of a pixel off its rect.
    echo "$pass: pane frames $(probe windowGeometry "$main" | jq -c .) $(probe windowGeometry "$neighbour" | jq -c .)"
    check "$pass: a Bento pair" context '.desktopStage.active'
    if [[ -n $sleeper ]]; then
        check "$pass: the sleeping window is a card, asleep" \
            context --arg id "$sleeper" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .minimized'
    fi
    kad showCardLine
    sleep 1
    report "$pass-spread"
    check "$pass: Spread shows the pair as one group" context '[.applications[] | select(.hasCard and .stackSize == 2)] | length == 2'

    # A tap on the group opens it as its layout.
    read -r gx gy < <(kad workspaceContext | jq -r '[.applications[] | select(.hasCard and .stackSize == 2 and .spreadRect)][0].spreadRect
        | "\(.x + .width / 2 | floor) \(.y + .height / 2 | floor)"')
    probe down "$touch" "$gx" "$gy"
    sleep .08
    probe up "$touch"
    # Each pane grows from where the group card drew it, smaller than its
    # place in the layout, rather than appearing there at once.
    growth=()
    for sample in {1..12}; do
        growth+=("$(kad nativeCarryState | jq -c --arg id "$main" \
            '[.bentoMotion[] | select(.window == $id)][0] // empty | {started, rect, target}')")
        sleep .02
    done
    printf '%s\n' "${growth[@]}" | grep . | sed "s/^/$pass: growth /"
    check "$pass: a pane grows from the group card into the layout" \
        jq -se 'map(select(.rect.width < .target.width)) | length > 0' < <(printf '%s\n' "${growth[@]}" | grep .)
    sleep 1.2
    report "$pass-opened"
    check "$pass: the tap opens the layout" context '.desktopStage.active and ([.applications[] | select(.hasCard and .stackSize == 2)] | length == 0)'

    if [[ $pass == closed ]]; then
        # The sleeping card closes while the layout is shown; the layout stays.
        client closeCompanion "Sleeper"
        sleep 1
        report "$pass-sleeper-closed"
        check "$pass: the layout stands once the sleeping card closes" context '.desktopStage.active'
        check "$pass: both panes are still on the display" context --arg m "$main" --arg n "$neighbour" \
            '[.applications[] | select((.windowId == $m or .windowId == $n) and (.minimized | not))] | length == 2'
    fi

    # The pane is dragged by its title bar to the top edge and let go.
    bounds=$(probe windowGeometry "$main")
    x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+40|floor' <<<"$bounds")
    probe pointer "$x" "$y"
    client armMove
    probe down $((touch + 1)) "$x" "$y"
    sleep .15
    carry=$(kad nativeCarryState)
    echo "$pass: picked up $(jq -c '{carrying, inputBusy}' <<<"$carry")"
    check "$pass: Kadunce carries the pane" jq -e '.carrying' <<<"$carry"
    for step in 300 160 60 12 2; do probe motion $((touch + 1)) "$x" "$step"; sleep .1; done
    probe up $((touch + 1))
    sleep 1.2
    report "$pass-top"
    check "$pass: let go at the top edge, the pane is the Active card" \
        context --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
    check "$pass: the layout is gone" context '(.desktopStage.active | not)'
    check "$pass: its partner is a card of its own" \
        context --arg id "$neighbour" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .stackSize == 1'
    if [[ $pass == sleeper ]]; then
        check "$pass: the sleeping card is still a card, asleep" \
            context --arg id "$sleeper" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .minimized'
    fi
    check "$pass: no ownership violations" context '(.ownershipViolations // []) | length == 0'

    test "$(probe releaseRuntime)" = true
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    probe contactDrop
    kill "$client_pid"; wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: spread bento top: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a Bento pair opened from Spread gives up a pane dragged to the top edge as the Active card'
