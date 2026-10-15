#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3, at sign-in: Kadunce takes the cards before the
# panels have taken their room, so each stands in the Active card's place
# the whole display gave. When a dock then reserves its height, every card
# stands in the place the area now gives, the ones behind the Active card
# too. Left where they were, they would reach the dock, which watches for
# windows reaching it (Shuffle's band does, as Plasma's adaptive panel does)
# and would stay opaque until each card had been brought forward once.
#
# Kadunce is loaded first and the windows open after it, as at sign-in; the
# client's main window opens maximized. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: sign-in reach: $name" >&2; failures=$((failures + 1)); fi
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce >/dev/null
sleep 1
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .8
client colouredCompanion 'Beside Two' 2e8b57 560 420
client colouredCompanion 'Beside Three' c88a1e 560 420
sleep 1.2
context=$(kad workspaceContext)
ids=$(jq -r '.applications[].windowId' <<<"$context")
bottom=$(jq '.displayContext.displays[] | select(.role == "tablet") | (.geometry.y + .geometry.height) | floor' <<<"$context")
echo "display bottom $bottom; presentation $(jq -r .cardStage.presentation <<<"$context"); cards $(wc -w <<<"$ids")"
check "three cards are taken" test "$(wc -w <<<"$ids")" -eq 3

# The dock arrives and takes its room.
client dockSurface 64
sleep 1.2
context=$(kad workspaceContext)
reaching=0
for id in $ids; do
    title=$(jq -r --arg id "$id" '.applications[] | select(.windowId == $id) | .title' <<<"$context")
    selected=$(jq -r --arg id "$id" '.applications[] | select(.windowId == $id) | .selected' <<<"$context")
    rect=$(probe windowGeometry "$id")
    edge=$(jq '.y + .height | floor' <<<"$rect")
    echo "card $title selected=$selected $rect reaches $edge"
    ((edge > bottom - 64)) && reaching=$((reaching + 1))
done
echo "cards reaching the dock: $reaching"
check "no card reaches the dock that arrived after the cards" test "$reaching" -eq 0
active=$(probe windowGeometry "$(jq -r 'first(.applications[] | select(.selected)) | .windowId' <<<"$context")")
for id in $ids; do
    check "card $id stands in the Active card's place" jq -n -e --argjson a "$active" --argjson b "$(probe windowGeometry "$id")" '$a == $b'
done

if ((failures)); then echo "FAIL: sign-in reach: $failures checks failed" >&2; exit 1; fi
echo "PASS: cards taken before the dock took its room all stand clear of it once it does"
