#!/usr/bin/env bash
# CARD-LIFECYCLE.md §3, at sign-in, as a session restores several windows at
# once: a browser opens, another window opens after it, the session brings
# the browser forward, and only then does the browser maximize itself as it
# was left. It is no longer the last window to have opened, but it is still
# opening: it stays the Active card in the Active place, nothing waits aside
# for a shown desktop, and no maximized window is left reaching the panels.
# Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: late maximize at sign-in: $name" >&2; failures=$((failures + 1)); fi
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
# The browser shows, and maximizes itself 900 ms later.
client lateMaximizedCompanion 'Restores maximized' 2e8b57 900
sleep .15
client colouredCompanion 'Opened after it' 4169e1 560 420
sleep .25
late=$(kad workspaceContext | jq -r '.applications[] | select(.title == "Restores maximized") | .windowId')
probe activateWindowId "$late" >/dev/null
sleep 2
context=$(kad workspaceContext)
bottom=$(jq '.displayContext.displays[] | select(.role == "tablet") | (.geometry.y + .geometry.height) | floor' <<<"$context")
echo "presentation $(jq -r .cardStage.presentation <<<"$context"); bottom $bottom; cards $(jq '.applications | length' <<<"$context"); focus $(jq -r .focus.title <<<"$context")"
check "the browser is a card" test -n "$late"
check "the browser, brought forward, is the Active card" jq -e '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .title == "Restores maximized")' <<<"$context"
if [[ -n $late ]]; then
    rect=$(probe windowGeometry "$late")
    echo "active $rect"
    check "it stands in the Active place, short of the bottom of the work area" jq -e --argjson b "$bottom" '.y + .height < $b - 2' <<<"$rect"
fi

test "$(probe releaseRuntime)" = true
sleep .5
if [[ -n $late ]]; then
    released=$(probe windowGeometry "$late")
    echo "released $released"
    check "release gives the browser back maximized, as its application asked" jq -e --argjson b "$bottom" '.y + .height >= $b - 2 and .width >= 1270' <<<"$released"
fi

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: late maximize at sign-in: $failures checks failed" >&2; exit 1; fi
echo "PASS: a window maximizing itself as it opens stays the Active card when others opened after it"
