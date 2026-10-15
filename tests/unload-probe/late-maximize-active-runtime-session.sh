#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3, at sign-in: the last window to arrive is the Active
# card, and its application, restoring its saved state, maximizes it a moment
# after showing it. That is the window opening as it was left, not a person
# asking it to fill the screen: it stays the Active card in the Active place,
# the cards behind it stay in the row rather than aside for a shown desktop,
# and release gives it back maximized. Past its opening a maximize is a
# person's, and lets the card go as before. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: late maximize Active: $name" >&2; failures=$((failures + 1)); fi
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
client colouredCompanion 'Opened first' c88a1e 560 420
sleep .8
client lateMaximizedCompanion 'Restores maximized' 2e8b57 300
sleep 2
context=$(kad workspaceContext)
bottom=$(jq '.displayContext.displays[] | select(.role == "tablet") | (.geometry.y + .geometry.height) | floor' <<<"$context")
echo "presentation $(jq -r .cardStage.presentation <<<"$context"); bottom $bottom; cards $(jq '.applications | length' <<<"$context")"
late=$(jq -r '.applications[] | select(.title == "Restores maximized") | .windowId' <<<"$context")
first=$(jq -r '.applications[] | select(.title == "Opened first") | .windowId' <<<"$context")
check "both windows are still cards" test -n "$late" -a -n "$first"
check "the window that maximized itself is the Active card" jq -e '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .title == "Restores maximized")' <<<"$context"
if [[ -n $late ]]; then
    rect=$(probe windowGeometry "$late")
    echo "active $rect"
    check "it stands in the Active place, short of the bottom of the work area" jq -e --argjson b "$bottom" '.y + .height < $b - 2' <<<"$rect"
fi

# Past its opening, a maximize is a person asking the card to fill the
# screen, and it goes as before.
probe maximizeCaption 'Restores maximized' >/dev/null
sleep 1
check "a later maximize still lets the Active card go" jq -e '.cardStage.presentation == "desktop"' <<<"$(kad workspaceContext)"

test "$(probe releaseRuntime)" = true
sleep .5
if [[ -n $late ]]; then
    released=$(probe windowGeometry "$late")
    echo "released $released"
    check "release gives the window back maximized, as its application asked" jq -e --argjson b "$bottom" '.y + .height >= $b - 2 and .width >= 1270' <<<"$released"
fi

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: late maximize Active: $failures checks failed" >&2; exit 1; fi
echo "PASS: the Active card maximizing itself as it opens stays the Active card, and release gives it back maximized"
