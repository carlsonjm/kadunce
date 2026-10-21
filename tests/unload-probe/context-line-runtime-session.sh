#!/usr/bin/env bash
# TETTEGOUCHE-CONTEXT.md § Context-line claims: a surface that claims a
# display's context line is given what Kadunce would have drawn there in the
# workspace context, and each change is announced: Spread's chosen entry, with
# a sleeping card's state as it is chosen, the monitors' zones as they
# switch, and why Meta+B did nothing, said for a while and then gone. The claim ends with its caller.
#
# Needs the tablet fixture. Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
here=$(cd "$(dirname "$0")" && pwd)
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
context() { kad workspaceContext; }
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: context line: $name" >&2; failures=$((failures + 1)); fi
}
is() { test "$1" = "$2"; }
signals_during() {
    local log; log=$(mktemp)
    dbus-monitor --session "type='signal',interface='co.goodinput.Kadunce',member='workspaceContextChanged'" >"$log" 2>/dev/null &
    local monitor=$!
    sleep .3
    : >"$log"
    "$@"
    sleep .4
    kill "$monitor" 2>/dev/null; wait "$monitor" 2>/dev/null
    grep -c 'member=workspaceContextChanged' "$log"
    rm -f "$log"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
claimer_pid=
trap 'kill "$client_pid" $claimer_pid 2>/dev/null || true' EXIT
sleep 1
probe contactFocus >/dev/null
client colouredCompanion 'One' c0392b 500 360
client colouredCompanion 'Two' 2980b9 500 360
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
probe minimizeWindow "$(id_of 'One')" true >/dev/null
sleep .3
probe activateWindowId "$(id_of 'Two')" >/dev/null
sleep .5

tablet=$(context | jq -r 'first(.displayContext.displays[] | select(.role == "tablet")) | .name')
check "the tablet display is named" test -n "$tablet"
python3 "$here/context-claimer.py" "$tablet" 60 >"$XDG_RUNTIME_DIR/claimer.log" 2>&1 &
claimer_pid=$!
for _ in {1..30}; do grep -q . "$XDG_RUNTIME_DIR/claimer.log" && break; sleep .1; done
check "Kadunce takes the claim" grep -qx claimed "$XDG_RUNTIME_DIR/claimer.log"

kad showCardLine >/dev/null
sleep 1
entry=$(context | jq -c '.cardStage.spreadEntry')
echo "spread entry: $entry"
check "Spread names its chosen entry" test -n "$(jq -r '.name // empty' <<<"$entry")"
check "the entry is the chosen card" is "$(jq -r '.cardId' <<<"$entry")" "$(context | jq -r '.cardStage.selectedCardId')"
check "a lone card has no Stack place" is "$(jq -r '"\(.stackPosition)/\(.stackSize)"' <<<"$entry")" 1/1
check "the chosen card is awake" is "$(jq -r '.sleeping' <<<"$entry")" false
check "nothing is noticed" is "$(context | jq -c '.notice')" null

# Choosing the sleeping card says so, and is announced.
chosen=$(signals_during probe key 105 0)
sleep .3
entry=$(context | jq -c '.cardStage.spreadEntry')
echo "after Left: $entry, $chosen signals"
check "choosing another entry is announced" test "$chosen" -ge 1
check "the sleeping card is chosen" is "$(jq -r '.cardId' <<<"$entry")" "$(id_of 'One' | tr -d '{}')"
check "and says it sleeps" is "$(jq -r '.sleeping' <<<"$entry")" true

# Meta+Shift+B: the zones are said rather than shown in a popup.
switched=$(signals_during probe keyHeld 48 1 125 42)
notice=$(context | jq -c '.notice')
echo "zones: $notice, $switched signals"
check "switching the zones is announced" test "$switched" -ge 1
check "the zones are noticed" is "$(jq -r '.kind' <<<"$notice")" zones
check "in words" bash -c "[[ \$(jq -r '.text' <<<'$notice') == Monitors:* ]]"
sleep 2.8
check "the notice is gone after its time" is "$(context | jq -c '.notice')" null
probe keyHeld 48 1 125 42 >/dev/null
sleep .3

# Meta+B with every other card asleep does nothing, and says why.
for id in $(probe windowFacts | jq -r --arg two "$(id_of 'Two')" \
        '.[] | select(.class == "unload-client" and .id != $two) | .id'); do
    probe minimizeWindow "$id" true >/dev/null
done
sleep .5
probe key 48 125 >/dev/null
sleep .5
notice=$(context | jq -c '.notice')
echo "Meta+B: $notice"
check "a key that did nothing is hinted" is "$(jq -r '.kind' <<<"$notice")" hint
check "with its reason" is "$(jq -r '.text' <<<"$notice")" "Nothing to pair it with"
check "and nothing paired" is "$(context | jq -r '.cardStage.presentation')" cardLine
sleep 2.8

# The claim goes with its caller; the context still says the entry.
kill "$claimer_pid" 2>/dev/null; wait "$claimer_pid" 2>/dev/null; claimer_pid=
sleep .5
check "the entry is still said without a claim" test -n "$(context | jq -r '.cardStage.spreadEntry.name // empty')"
probe key 106 0 >/dev/null
sleep .3
kad showActive >/dev/null
sleep .8
check "outside Spread there is no entry" is "$(context | jq -c '.cardStage.spreadEntry')" null

(( failures == 0 )) || exit "$failures"
echo 'PASS: a claimed context line is told Spread'"'"'s entry, a sleeping card, the zones and a hint, each as it changes'
