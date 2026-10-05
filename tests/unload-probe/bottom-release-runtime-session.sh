#!/usr/bin/env bash
# CARD-LIFECYCLE.md §10 and §13: a carried window let go at the bottom edge
# returns to the ordinary Plasma desktop, whatever Kadunce held it as, and
# nothing else changes. Here the Active card is carried by pointer and by
# finger, and a Bento pane by pointer. The window stays on the tablet, shown
# and not minimized, and is no longer a card or a pane; the other window is
# still a card.
#
# A card held in Spread and let go at the bottom edge is reported, not checked:
# pulling a held card down is how Spread zooms out (INPUT.md § Spread).
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
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: bottom release: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad outputStageState | tr '\n' ' ') $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {title, hasCard, selected, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.normal) | {caption, output, minimized, hidden, x, y, width, height})')"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
# KWin opens a window on the display under the pointer.
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client ordinaryCompanion
sleep .8
main=$(probe windowFacts | jq -r 'first(.[] | select(.caption == "unload-client")) | .id')
other=$(probe windowFacts | jq -r 'first(.[] | select(.caption == "Ordinary neighbor probe")) | .id')
probe contactObserve
probe contactStart >/dev/null

notCard() { context --arg id "$1" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 0'; }
isCard() { context --arg id "$1" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 1'; }
noPanes() { kad outputStageState | grep -q '^Virtual-0|.*|0$'; }
paired() { kad outputStageState | grep -q '^Virtual-0|.*|2$'; }
# Shown on the tablet, wholly on it, and not minimized.
onDesktop() { probe windowFacts | jq -e --arg id "$1" 'first(.[] | select(.id == $id))
    | .output == "Virtual-0" and (.minimized | not) and (.hidden | not)
      and .x >= 0 and .y >= 0 and .x + .width <= 1280 and .y + .height <= 800'; }

# Carries the application's own window from its middle to the bottom edge and
# lets go there, by pointer or by finger.
carry() {
    local kind=$1 px py
    read -r px py < <(probe windowGeometry "$main" | jq -r '"\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
    probe contactFocus >/dev/null
    probe pointer "$px" "$py"
    client armMove
    if [[ $kind == pointer ]]; then probe contactButton true; else probe down 91 "$px" "$py"; fi
    sleep .15
    if [[ $kind == pointer ]]; then probe contactMotion "$px" 600; else probe motion 91 "$px" 600; fi
    sleep .15
    if [[ $kind == pointer ]]; then probe contactMotion "$px" 796; else probe motion 91 "$px" 796; fi
    sleep .3
    echo "at the bottom edge: $(kad nativeCarryState | jq -c '{carrying, destination, destinationPreview, detachPreview}')"
    if [[ $kind == pointer ]]; then probe contactButton false; else probe up 91; fi
    sleep 1
    echo "carry trace: $(kad nativeMoveTrace | jq -sc 'map(.event) | .[-4:]')"
}

for scenario in active-pointer active-touch pane; do
    probe contactFocus >/dev/null
    sleep .2
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    if [[ $scenario == pane ]]; then
        # §3's Bento action pairs the Active card, on the left, with the other.
        test "$(kad toggleBentoOnOutput Virtual-0)" = true
        sleep 1
        check "$scenario: before, the two windows are a Bento pair" paired
    else
        check "$scenario: before, the application window is the Active card" \
            context --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
    fi
    report "$scenario-before"
    carry "${scenario#active-}"
    report "$scenario-released"
    check "$scenario: it is no longer a card" notCard "$main"
    check "$scenario: it is no pane" noPanes
    check "$scenario: it is shown on the tablet, not minimized" onDesktop "$main"
    check "$scenario: the other window is still a card" isCard "$other"
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .8
done

# Reported only: a card held in Spread and let go at the bottom edge.
probe contactFocus >/dev/null
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
kad showCardLine
sleep .8
read -r sx sy < <(kad workspaceContext | jq -r --arg id "$main" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
    | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
probe down 92 "$sx" "$sy"
sleep .4
probe motion 92 "$sx" "$((sy + 40))"
sleep .2
probe motion 92 "$sx" 796
sleep .6
echo "spread card at the bottom edge: $(kad nativeCarryState | jq -c '{lineCarrying, carryAim, carryScale}')"
probe up 92
sleep 1
report spread-released
echo "spread card let go at the bottom edge: $(kad workspaceContext | jq -c --arg id "$main" '{p: .cardStage.presentation, card: [.applications[] | select(.windowId == $id) | {hasCard, selected, minimized}]}')"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .5

if ((failures)); then echo "FAIL: bottom release: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a carried card or pane let go at the bottom edge returns to the desktop'
