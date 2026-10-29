#!/usr/bin/env bash
# INPUT.md § Active card: a swipe in the gap beside the Active card carries the
# row under the finger. Let go short of a quarter of the card's width, it goes
# back to the card it started on; past it, the next card is Active. A tap there
# turns one card, and at the row's end the card stays.
set -euo pipefail
trap 'echo "FAIL: active slide line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
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
client companion
client crossCompanion
client ordinaryCompanion
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
# This session requires the disposable Virtual-0 tablet predicate fixture.
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
selected() { kad workspaceContext | jq -r '.cardStage.selectedCardId'; }
active() { kad workspaceContext | jq -e '.cardStage.presentation == "active"' >/dev/null; }
width=$(kad workspaceContext | jq -r '.displayContext.displays[] | select(.name == "Virtual-0") | .geometry.width')
right=$((width - 3))
kad showActive
sleep .5
active
# Walk to the first card so the row has cards after it.
for step in 1 2 3; do probe down 51 3 600; probe up 51; sleep .4; done
first=$(selected)
echo "first card $first"

swipe() { # from the right gap, leftward by $1 pixels, in steps
    probe down 52 "$right" 600
    local x=$right
    for part in 1 2 3 4 5 6; do x=$((right - $1 * part / 6)); probe motion 52 "$x" 600; sleep .02; done
    sleep .15  # held still, so the release carries no flick
    probe motion 52 "$x" 600
    probe up 52
    sleep .45
}

swipe $((width / 8))
active
test "$(selected)" = "$first"
echo 'PASS: let go short of a quarter, the strip goes back to the card it started on'

swipe $((width * 2 / 5))
active
second=$(selected)
test "$second" != "$first"
echo 'PASS: past a quarter, the next card is Active'

probe down 53 "$right" 600; probe up 53
sleep .45
active
third=$(selected)
test "$third" != "$second" && test "$third" != "$first"
echo 'PASS: a tap in the gap turns one card'

# Walk on to the last card; there, a tap leaves it Active.
for step in 1 2 3 4; do probe down 54 "$right" 600; probe up 54; sleep .4; done
last=$(selected)
probe down 55 "$right" 600; probe up 55
sleep .45
active
test "$(selected)" = "$last"
echo 'PASS: at the row end the card stays Active'
