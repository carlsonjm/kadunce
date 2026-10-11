#!/usr/bin/env bash
# INPUT.md § Table and § Active card: a pull down from the top edge over a Bento
# layout opens Table, and a touch in the gutter above a pane resizes nothing.
# An application drawing its own title bar keeps an invisible resize border
# outside its frame, wider than the gutter above a pane, and a finger on the
# top edge landed on it: the window began a resize of its own and Table never
# came. Two GTK windows, which draw their own title bars, are the panes.
#
# Needs the tablet fixture, whose touchscreen gives Kadunce the direct edges. Every check is
# reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
table() { kad tableState | jq -r "$1"; }
frames() { probe windowFacts | jq -c '[.[] | select(.caption == "Pane A" or .caption == "Pane B") | {caption, x: (.x | floor), y: (.y | floor), width: (.width | floor), height: (.height | floor)}] | sort_by(.caption)'; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: bento top: $name" >&2; failures=$((failures + 1)); fi
}
is() { test "$1" = "$2"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .5
probe pointer 600 400
python3 "$(dirname "$0")/gtk-pane.py" 'Pane A' &
first_pid=$!
sleep .6
python3 "$(dirname "$0")/gtk-pane.py" 'Pane B' &
second_pid=$!
trap 'kill "$first_pid" "$second_pid" 2>/dev/null || true' EXIT
for attempt in {1..60}; do
    [[ $(probe windowFacts | jq '[.[] | select(.caption == "Pane A" or .caption == "Pane B")] | length') == 2 ]] && break
    sleep .1
done
sleep .5
probe activateWindowId "$(probe windowFacts | jq -r 'first(.[] | select(.caption == "Pane A")) | .id')" >/dev/null
sleep .5
test "$(kad toggleBentoOnOutput Virtual-0)" = true || { echo "FAIL: bento top: no layout" >&2; exit 1; }
sleep 1.2
twoPanes() { kad outputStageState | grep -q '^Virtual-0|.*|2$'; }
check "a layout of two panes" twoPanes
before=$(frames)
touch=70
for pane in 'Pane A' 'Pane B'; do
    touch=$((touch + 1))
    read -r x top < <(probe windowFacts | jq -r --arg c "$pane" 'first(.[] | select(.caption == $c)) | "\(.x + .width / 2 | floor) \(.y | floor)"')
    # The finger lands on the top edge, above the pane's frame, where its
    # invisible border reaches.
    y=$(( top > 3 ? top - 3 : 0 ))
    echo "$pane: frame top $top; finger at $x $y; in its border $(probe inMargin "$pane" "$x" "$y")"
    check "$pane: its border reaches above its frame" is "$(probe inMargin "$pane" "$x" "$y")" true
    probe down "$touch" "$x" "$y"
    for step in 14 28 44 56 70; do probe motion "$touch" "$x" $((y + step)); sleep .03; done
    sleep .3
    pulled=$(frames)
    echo "$pane: pulled $(kad tableState | jq -c '{open, level}') panes $pulled"
    check "$pane: a pull from above it opens Table" is "$(table '.open')" true
    check "$pane: the pull resizes nothing" is "$pulled" "$before"
    # Pushed back to the edge, the lift cancels and changes nothing.
    probe motion "$touch" "$x" 20; sleep .1; probe motion "$touch" "$x" 4; sleep .1
    probe up "$touch"
    sleep .6
    check "$pane: pushed back up, Table closes" is "$(table '.open')" false
    check "$pane: the panes were not resized" is "$(frames)" "$before"
    # A touch there that does not pull leaves everything as it was.
    probe down "$touch" "$x" "$y"
    probe motion "$touch" $((x + 60)) "$y"
    sleep .1
    probe up "$touch"
    sleep .5
    check "$pane: a stroke along the top edge resizes nothing" is "$(frames)" "$before"
    check "$pane: and opens nothing" is "$(table '.open')" false
done
check "the layout stands" twoPanes

if ((failures)); then echo "FAIL: bento top: $failures checks failed" >&2; exit 1; fi
echo 'PASS: over a Bento layout a pull from the top edge opens Table, and a touch above a pane resizes nothing'
