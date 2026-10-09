#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3: every card Kadunce takes stands in the Active card's
# place, not only the one presented. A maximized window behind the Active card
# would otherwise reach the bottom of the work area, where a panel watching
# for windows that reach it (Shuffle's band does, as Plasma's adaptive panel
# does) stays opaque until each card has been brought forward once. Release
# gives every window back its own place.
#
# The client's main window opens maximized; two smaller windows open after it,
# so the last of them is the one presented. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: adopt reach: $name" >&2; failures=$((failures + 1)); fi
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .8
client colouredCompanion 'Beside Two' 2e8b57 560 420
client colouredCompanion 'Beside Three' c88a1e 560 420
sleep .8
ids=$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce >/dev/null; sleep 1.2; kad workspaceContext | jq -r '.applications[].windowId')
context=$(kad workspaceContext)
bottom=$(jq '.displayContext.displays[] | select(.role == "tablet") | (.geometry.y + .geometry.height) | floor' <<<"$context")
echo "work area bottom $bottom; presentation $(jq -r .cardStage.presentation <<<"$context")"
reaching=0
for id in $ids; do
    title=$(jq -r --arg id "$id" '.applications[] | select(.windowId == $id) | .title' <<<"$context")
    selected=$(jq -r --arg id "$id" '.applications[] | select(.windowId == $id) | .selected' <<<"$context")
    rect=$(probe windowGeometry "$id")
    edge=$(jq '.y + .height | floor' <<<"$rect")
    echo "card $title selected=$selected $rect reaches $edge"
    ((edge >= bottom - 2)) && reaching=$((reaching + 1))
done
echo "cards reaching the bottom edge: $reaching"
check "no card reaches the bottom of the work area" test "$reaching" -eq 0
active=$(probe windowGeometry "$(jq -r 'first(.applications[] | select(.selected)) | .windowId' <<<"$context")")
for id in $ids; do
    check "card $id stands in the Active card's place" jq -n -e --argjson a "$active" --argjson b "$(probe windowGeometry "$id")" '$a == $b'
done

# Release gives the maximized window back as it was.
main=$(jq -r '.applications[] | select(.title == "unload-client") | .windowId' <<<"$context")
test "$(probe releaseRuntime)" = true
sleep .5
released=$(probe windowGeometry "$main")
echo "released main window $released"
check "the maximized window is given back reaching its own bottom" jq -e --argjson b "$bottom" '.y + .height >= $b - 2' <<<"$released"

if ((failures)); then echo "FAIL: adopt reach: $failures checks failed" >&2; exit 1; fi
echo "PASS: every card taken stands in the Active card's place, none reaching the bottom edge, and release gives each window back"
