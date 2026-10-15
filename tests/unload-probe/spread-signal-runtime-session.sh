#!/usr/bin/env bash
# TETTEGOUCHE-CONTEXT.md: the workspace context is announced on the first frame
# after the presentation it reports changes, whichever way Spread opens or
# closes, so a dock's Spread button and the Context Bar follow at once rather
# than on a re-read.
#
# Needs the tablet fixture. Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
presentation() { kad workspaceContext | jq -r '.cardStage.presentation'; }
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: spread signal: $name" >&2; failures=$((failures + 1)); fi
}
is() { test "$1" = "$2"; }
# Counts the context signals sent while "$@" runs and for a short while after.
signals_during() {
    local log; log=$(mktemp)
    dbus-monitor --session "type='signal',interface='co.goodinput.Kadunce',member='workspaceContextChanged'" >"$log" 2>/dev/null &
    local monitor=$!
    sleep .3
    : >"$log"
    "$@"
    sleep .25
    kill "$monitor" 2>/dev/null; wait "$monitor" 2>/dev/null
    grep -c 'member=workspaceContextChanged' "$log"
    rm -f "$log"
}
escape() { probe key 1 0; }
tap() { probe down 91 "$1" "$2"; sleep .05; probe up 91; }
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

for way in showActive escape tap; do
    opened=$(signals_during kad showCardLine)
    sleep .8
    check "$way: Spread is open" is "$(presentation)" cardLine
    check "$way: opening Spread is announced" test "$opened" -ge 1
    case $way in
        showActive) closed=$(signals_during kad showActive) ;;
        escape) closed=$(signals_during escape) ;;
        tap)
            # The centred card, which a tap chooses.
            rect=$(kad workspaceContext | jq -r --arg id "$(id_of 'Two')" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
                | "\((.x + .width / 2) | floor) \((.y + .height / 2) | floor)"')
            # shellcheck disable=SC2086
            closed=$(signals_during tap $rect) ;;
    esac
    sleep .8
    echo "$way: $opened opening, $closed closing signals; now $(presentation)"
    check "$way: Spread is closed" test "$(presentation)" != cardLine
    check "$way: closing Spread is announced" test "$closed" -ge 1
done

(( failures == 0 )) || exit "$failures"
echo 'PASS: opening and closing Spread each announce the workspace context, however Spread closes'
