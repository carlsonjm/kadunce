#!/usr/bin/env bash
# CARD-LIFECYCLE.md §5, §10 and §3 on the display that can own cards: a Bento
# pane let go at the bottom edge returns to the desktop, and the pane left
# behind is an individual card. Asked for as the dock asks, it shows as the
# Active card, never in its old pane. The returned window, snapped to a side
# edge, pairs with it, and Spread then shows that pair as one group.
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
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: bento exit partner: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId, bento: .desktopStage.active, apps: [.applications[] | {windowId, hasCard, stackSize, selected}]}')"
    echo "frames $1 main $(probe windowGeometry "$main" | jq -c .) neighbour $(probe windowGeometry "$neighbour" | jq -c .)"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
probe contactStart >/dev/null
client colouredCompanion "Neighbour" "c03a3a" 700 500
sleep .8
neighbour=$(probe windowIdByCaption "Neighbour")
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
main=$(kad workspaceContext | jq -r --arg n "$neighbour" '[.applications[] | select(.windowId != $n)][0].windowId')
probe contactFocus >/dev/null
sleep .5
card=$(probe windowGeometry "$main" | jq -c .)
check "the Active card" context --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'

# The Active card and its neighbour become a Bento pair.
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .8
report paired
check "a Bento pair" context '.desktopStage.active'

# The pane under the finger is carried to the bottom edge and let go.
bounds=$(probe windowGeometry "$main")
x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+40|floor' <<<"$bounds")
probe pointer "$x" "$y"
client armMove
probe down 71 "$x" "$y"
sleep .15
for step in 500 650 760 799; do probe motion 71 "$x" "$step"; sleep .1; done
probe up 71
sleep 1
report released
check "released: the desktop shows, the layout gone" context '.cardStage.presentation == "desktop" and (.desktopStage.active | not)'

# Asked for as the dock asks, the pane left behind is the Active card.
probe activateWindowId "$neighbour" >/dev/null
sleep 1
report asked
check "asked: the partner is the Active card" context --arg id "$neighbour" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id and (.desktopStage.active | not)'
check "asked: it fills the card, not its old pane" test "$(probe windowGeometry "$neighbour" | jq -c .)" = "$card"

# The returned window, carried to the left edge, pairs with the Active card.
probe activateWindowId "$main" >/dev/null
sleep .8
report main-asked
bounds=$(probe windowGeometry "$main")
x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+40|floor' <<<"$bounds")
probe pointer "$x" "$y"
client armMove
probe down 72 "$x" "$y"
sleep .15
probe motion 72 650 400; sleep .15
probe motion 72 5 400; sleep .3
probe up 72
sleep 1
report snapped
check "snapped: a Bento pair again" context '.desktopStage.active'

# Spread shows that pair as one group card.
kad showCardLine
sleep 1
report spread
check "Spread: the pair is one group" context '[.applications[] | select(.hasCard and .stackSize == 2)] | length == 2'
check "Spread: no pane stands alone" context '[.applications[] | select(.hasCard and .stackSize == 1)] | length == 0'

check "no ownership violations" context '(.ownershipViolations // []) | length == 0'
if ((failures)); then echo "FAIL: bento exit partner: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a pane let go at the bottom edge leaves its partner a card, shown whole when asked for, and pairing again makes one group'
