#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3, at sign-in: windows arrive one by one after Kadunce
# holds the display, and an application restoring its saved state maximizes
# its window a moment after showing it, by which time the next window may
# stand in front of it. A card behind the Active one must still stand in the
# Active card's place, not reach the bottom of the work area, where a panel
# watching for windows that reach it stays opaque. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: late maximize: $name" >&2; failures=$((failures + 1)); fi
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
# Kadunce is on before anything opens, as at sign-in.
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .5
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client lateMaximizedCompanion 'Restores maximized' 2e8b57 700
sleep .3
client colouredCompanion 'Opens after' c88a1e 560 420
sleep 2
context=$(kad workspaceContext)
bottom=$(jq '.displayContext.displays[] | select(.role == "tablet") | (.geometry.y + .geometry.height) | floor' <<<"$context")
echo "presentation $(jq -r .cardStage.presentation <<<"$context"); bottom $bottom"
reaching=0
for id in $(jq -r '.applications[].windowId' <<<"$context"); do
    title=$(jq -r --arg id "$id" '.applications[] | select(.windowId == $id) | .title' <<<"$context")
    selected=$(jq -r --arg id "$id" '.applications[] | select(.windowId == $id) | .selected' <<<"$context")
    rect=$(probe windowGeometry "$id")
    edge=$(jq '.y + .height | floor' <<<"$rect")
    echo "card $title selected=$selected $rect reaches $edge"
    ((edge >= bottom - 2)) && reaching=$((reaching + 1))
done
check "no card reaches the bottom of the work area" test "$reaching" -eq 0
check "the last window opened is the Active card" jq -e '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .title == "Opens after")' <<<"$context"

# The application asked for maximized while held: release gives it that.
late=$(jq -r '.applications[] | select(.title == "Restores maximized") | .windowId' <<<"$context")
test "$(probe releaseRuntime)" = true
sleep .5
released=$(probe windowGeometry "$late")
echo "released $released"
check "release gives the window back maximized, as its application asked" jq -e --argjson b "$bottom" '.y + .height >= $b - 2 and .width >= 1270' <<<"$released"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: late maximize: $failures checks failed" >&2; exit 1; fi
echo "PASS: a card that maximizes itself behind the Active card still stands in the Active card's place, and release gives it back maximized"
