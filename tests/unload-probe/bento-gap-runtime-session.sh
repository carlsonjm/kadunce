#!/usr/bin/env bash
# The space between Bento's panes is the card gutter once one is set, and 14
# until then; a change to it moves the standing panes at once, never closer
# than 4, and the layout stands through every change.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: bento gap: $name" >&2; failures=$((failures + 1)); fi
}
frames() { probe windowFacts | jq -c '[.[] | select(.caption == "Pane A" or .caption == "Pane B") | {caption, x: (.x | floor), y: (.y | floor), width: (.width | floor), height: (.height | floor)}] | sort_by(.caption)'; }
# The space between the two panes, along whichever way they split.
between() {
    frames | jq -r 'if length != 2 then "none" else . as [$a, $b] |
        if ($b.x - $a.x | fabs) > ($b.y - $a.y | fabs)
        then ([$a, $b] | sort_by(.x) | .[1].x - (.[0].x + .[0].width))
        else ([$a, $b] | sort_by(.y) | .[1].y - (.[0].y + .[0].height)) end end'
}
spacing() {
    [[ -z $1 ]] || kwriteconfig6 --file kwinrc --group Effect-kadunce --key ActiveCardGutter "$1" --notify
    for _ in {1..30}; do [[ $(between) == "$2" ]] && return; sleep .1; done
    echo "gutter ${1:-unset}: panes $(between) apart, expected $2" >&2
    false
}
twoPanes() { kad outputStageState | grep -q '^Virtual-0|.*|2$'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client colouredCompanion "Pane A" "c03a3a" 560 420
client colouredCompanion "Pane B" "2e8b57" 560 420
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
probe activateWindowId "$(probe windowIdByCaption "Pane A")" >/dev/null
sleep .5
test "$(kad toggleBentoOnOutput Virtual-0)" = true || { echo "FAIL: bento gap: no layout" >&2; exit 1; }
sleep 1.2
check "a layout of two panes" twoPanes
echo "panes before: $(frames)"
check "with no gutter set the panes stand 14 apart" spacing "" 14
check "a gutter of 4 brings them 4 apart at once" spacing 4 4
check "a gutter under 4 keeps them 4 apart" spacing 2 4
check "a gutter of 24 moves them 24 apart" spacing 24 24
check "a gutter of 10 moves them 10 apart" spacing 10 10
echo "panes after: $(frames)"
check "the layout stands through every change" twoPanes

if ((failures)); then echo "FAIL: bento gap: $failures checks failed" >&2; exit 1; fi
echo 'PASS: Bento panes stand the card gutter apart once one is set, at once, never under 4'
