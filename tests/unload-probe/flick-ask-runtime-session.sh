#!/usr/bin/env bash
# INPUT.md § Spread: a card flicked up closes its application, and one that
# asks first comes forward with its question, whether in a box of its own or
# drawn inside its window. The card does not close: it opens as the Active card
# with the question over it, by touch and by pointer.
# A card whose application closes without asking is gone and Spread stays.
#
# Needs the tablet fixture: only a display that can own cards presents Spread.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: flick ask: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
question() { probe windowFacts | jq -e --arg p "$1" 'any(.[]; .caption == "Unsaved probe" and .parentId == $p and (.hidden | not))'; }
press() { if [[ $kind == pointer ]]; then probe pointer "$1" "$2"; probe contactButton true; else probe down "$touch" "$1" "$2"; fi; }
drag() { if [[ $kind == pointer ]]; then probe contactMotion "$1" "$2"; else probe motion "$touch" "$1" "$2"; fi; }
lift() { if [[ $kind == pointer ]]; then probe contactButton false; else probe up "$touch"; fi; }
# A quick stroke up from the middle of the centred card.
flick() {
    local rect x y
    rect=$(kad workspaceContext | jq -c 'first(.applications[] | select(.selected and .spreadRect)) | .spreadRect')
    x=$(jq '(.x + .width / 2) | floor' <<<"$rect") y=$(jq '(.y + .height / 2) | floor' <<<"$rect")
    press "$x" "$y"
    sleep .05
    drag "$x" "$((y - 12))"
    sleep .02
    drag "$x" "$((y - 200))"
    lift
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
touch=70
for kind in touch pointer; do
    touch=$((touch + 1))
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    client askingCompanion 'Asks First' c88a1e
    client colouredCompanion 'Closes Quietly' 2e8b57 560 420
    client insideAskingCompanion 'Asks Inside' 3a6ec8
    sleep .8
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    asks=$(idOf 'Asks First') quiet=$(idOf 'Closes Quietly') inside=$(idOf 'Asks Inside')
    echo "$kind: asks $asks, closes quietly $quiet, asks inside $inside"

    # The window that closes without asking: gone, and Spread stays.
    probe activateWindowId "$quiet" >/dev/null
    sleep .6
    kad showCardLine
    sleep .8
    check "$kind: the quiet card is centred" context --arg id "$quiet" '.cardStage.selectedCardId == $id'
    flick
    sleep 1.5
    check "$kind: the quiet card's application closed" \
        context --arg id "$quiet" '[.applications[] | select(.windowId == $id)] | length == 0'
    check "$kind: Spread stays after it closes" context '.cardStage.presentation == "cardLine"'

    # The window that asks first: it stays, and comes forward with its question.
    probe activateWindowId "$asks" >/dev/null
    sleep .6
    kad showCardLine
    sleep .8
    check "$kind: the asking card is centred" context --arg id "$asks" '.cardStage.selectedCardId == $id'
    flick
    sleep 1.8
    echo "$kind: after the flick $(kad workspaceContext | jq -c '{p: .cardStage.presentation, selected: .cardStage.selectedCardId}') $(probe windowFacts | jq -c '[.[] | select(.caption == "Unsaved probe") | {parentId, hidden, active}]')"
    check "$kind: the asking card's application is still open" \
        context --arg id "$asks" 'any(.applications[]; .windowId == $id and .hasCard)'
    check "$kind: it comes forward as the Active card" \
        context --arg id "$asks" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
    check "$kind: its question shows over it" question "$asks"

    client closeDialogs
    sleep .3

    # The window that asks inside itself, as a terminal with a running program
    # does: no box of its own, only what it draws. It comes forward as soon as
    # it is plainly staying, not after the wait for an application that hangs.
    probe activateWindowId "$inside" >/dev/null
    sleep .6
    kad showCardLine
    sleep .8
    check "$kind: the inside asker's card is centred" context --arg id "$inside" '.cardStage.selectedCardId == $id'
    flick
    started=$(date +%s%N) waited=
    while (( ($(date +%s%N) - started) / 1000000 < 3000 )); do
        if context --arg id "$inside" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id' >/dev/null 2>&1; then
            waited=$(( ($(date +%s%N) - started) / 1000000 ))
            break
        fi
        sleep .05
    done
    echo "$kind: the inside asker came forward after ${waited:-more than 3000} ms"
    check "$kind: the inside asker's application is still open" \
        context --arg id "$inside" 'any(.applications[]; .windowId == $id and .hasCard)'
    check "$kind: the inside asker comes forward as the Active card within a second" \
        test "${waited:-9999}" -lt 1000
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: flick ask: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card flicked up whose application asks first comes forward with its question'
