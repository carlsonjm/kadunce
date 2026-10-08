#!/usr/bin/env bash
# INPUT.md § Bento on the display that can own cards: the pointer over the
# divider between a Bento pair takes its resize shape, a mouse pressed there
# drags it at once, a click alone changes nothing, and a finger that rests
# first drags it too.
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
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: tablet divider: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, bento: .desktopStage.active}')"
    echo "frames $1 main $(probe windowGeometry "$main" | jq -c .) neighbour $(probe windowGeometry "$neighbour" | jq -c .)"
}
wider() { probe windowGeometry "$main" | jq -e --argjson old "$1" '.width > $old.width+20'; }
same() { test "$(probe windowGeometry "$main")" = "$1"; }
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
client colouredCompanion "Neighbour" "c03a3a" 700 500
sleep .8
neighbour=$(probe windowIdByCaption "Neighbour")
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
main=$(kad workspaceContext | jq -r --arg n "$neighbour" '[.applications[] | select(.windowId != $n)][0].windowId')
probe contactFocus >/dev/null
sleep .5
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .8
report paired
check "a Bento pair" context '.desktopStage.active'

# The divider's point, a little below the top so no edge claims it.
divider() {
    local a b
    a=$(probe windowGeometry "$main"); b=$(probe windowGeometry "$neighbour")
    jq -nr --argjson a "$a" --argjson b "$b" \
        '(if $a.x < $b.x then [$a,$b] else [$b,$a] end) as [$l,$r]
         | [($l.x+$l.width+$r.x)/2,($l.y+$l.height*.3)] | map(floor) | @tsv'
}
# Moving the divider away from the main pane widens it.
step=40
if ! jq -e --argjson b "$(probe windowGeometry "$neighbour")" '.x < $b.x' <<<"$(probe windowGeometry "$main")" >/dev/null; then step=-40; fi
read -r rx ry <<<"$(divider)"

# The divider's whole reach, a pane's edge included, is held with one resize
# shape.
probe pointer "$rx" "$ry"; sleep .15; probe pointer "$rx" "$((ry+1))"; sleep .15
check "the divider holds the pointer" test "$(probe mouseIntercepted)" = true
check "the divider shows a column resize" test "$(probe pointerShape)" = col-resize
probe pointer "$((rx-12))" "$ry"; sleep .15
check "a pane's edge by the divider shows the same resize" test "$(probe pointerShape)" = col-resize

# A mouse pressed and moved off in one go.
before=$(probe windowGeometry "$main")
probe pointer "$rx" "$ry"
probe contactPressAndMove "$((rx+step))" "$ry"
probe contactButton false
sleep .6
report mouse
check "a mouse drags the divider at once" wider "$before"

# A click alone.
read -r rx ry <<<"$(divider)"
before=$(probe windowGeometry "$main")
probe pointer "$rx" "$ry"
probe contactButton true
probe contactButton false
sleep .4
check "a click on the divider changes nothing" same "$before"

# A finger that rests first.
read -r rx ry <<<"$(divider)"
before=$(probe windowGeometry "$main")
probe down 81 "$rx" "$ry"
sleep .2
probe motion 81 "$((rx+step))" "$ry"
probe up 81
sleep .6
report touch
check "a finger drags the divider after a rest" wider "$before"

if (( failures )); then exit 1; fi
echo 'PASS: tablet Bento divider follows a mouse at once and a finger after a rest'
