#!/usr/bin/env bash
# TETTEGOUCHE-CONTEXT.md § Who may call: a program outside the suite asks what
# Kadunce shows but cannot bring anything forward; one of the suite's programs,
# run from the person's own programs folder, can.
#
# Runs with caller checks on, and HOME a folder of the session's own.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
[[ ${HOME:-} == /tmp/kadunce-unload-*/home ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
presentation() { kad workspaceContext | jq -r '.cardStage.presentation'; }
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
active_caption() { probe windowFacts | jq -r 'first(.[] | select(.active)) | .caption'; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: caller fence: $name" >&2; failures=$((failures + 1)); fi
}
is() { test "$1" = "$2"; }
refused() { local said; if said=$("$@" 2>&1); then return 1; fi; grep -q 'AccessDenied\|Only the suite' <<<"$said"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactFocus >/dev/null
client colouredCompanion 'One' c0392b 500 360
client colouredCompanion 'Two' 2980b9 500 360
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
probe activateWindowId "$(id_of 'Two')" >/dev/null
sleep .5
before=$(presentation)

check "a stranger reads the workspace" test -n "$before"
check "a stranger cannot open Spread" refused kad showCardLine
sleep .5
check "Spread stays as it was" is "$(presentation)" "$before"
check "a stranger cannot bring a window forward" refused kad activateApplicationWindow "$(id_of 'One')"
sleep .3
check "the window in front stays in front" is "$(active_caption)" Two
check "a stranger cannot raise the keys" refused kad raiseKeyboard
check "a stranger cannot open Table" refused kad toggleTable
check "a stranger cannot aim a placement" refused kad aimPlacement 600 400

# One of the suite's programs, by its own name in the person's programs folder.
mkdir -p "$HOME/.local/bin"
cp -L "$(command -v qdbus6)" "$HOME/.local/bin/tettegouche"
suite() { "$HOME/.local/bin/tettegouche" org.kde.KWin /Kadunce "$@"; }
check "Search brings a window forward" is "$(suite activateApplicationWindow "$(id_of 'One')")" true
sleep .5
check "that window is in front" is "$(active_caption)" One
# The same program under another name is a stranger.
cp -L "$(command -v qdbus6)" "$HOME/.local/bin/not-tettegouche"
check "a renamed copy is a stranger" refused "$HOME/.local/bin/not-tettegouche" org.kde.KWin /Kadunce showCardLine

(( failures == 0 )) || exit "$failures"
echo 'PASS: a stranger reads but cannot drive Kadunce, and a suite program in the programs folder can'
