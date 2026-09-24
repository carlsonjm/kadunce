#!/usr/bin/env bash
# The keyboard overlays the desktop. KWin starts a real input-method client for
# this session, and probe windows hold a focused text field at their bottom
# edge, at their top edge, and one reports no cursor at all. Showing and hiding
# the keyboard must change no card's size; a covered cursor lifts only the
# Active card, only as far as it needs, and the card returns exactly.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
set -euo pipefail
trap 'echo "FAIL: keyboard runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
state() { probe keyboardState; }
frame() { probe frames | jq -c --arg t "$1" '.[$t]'; }
record() { printf '%s %s %s\n' "$1" "$(state)" "$(probe frames)"; }
raise() { kad raiseKeyboard; sleep 1; }
# How much of a band of the display is the reveal probe's own colour.
band() { python3 "$(dirname "$0")/capture-band.py" "$@" 2a6f97; }
within() { awk -v v="$1" -v lo="$2" -v hi="$3" 'BEGIN { exit !(v >= lo && v <= hi) }'; }
lower() { probe hideKeyboard; sleep 1.5; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
QT_IM_MODULE=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for attempt in {1..40}; do
    [[ $(vk available 2>/dev/null) == true ]] && break
    sleep .1
done
test "$(vk available)" = true
client textCompanion
client topTextCompanion
client blindTextCompanion
sleep 1
test "$(state | jq '.overlayOption')" = false

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
state | jq -e '.overlayOption == true'
echo 'PASS: the compositor does not lift a window for the keyboard while Kadunce is loaded'

present() {
    local title=$1
    kad showCardLine
    sleep .4
    client focusText "$title"
    sleep .4
    kad showActive
    sleep 1
    client focusText "$title"
    sleep .5
    state | jq -e --arg t "$title" '.tracked == $t'
    frame "$title" | jq -e '.width == 1260 and .height == 780'
}

# A field at the bottom of an Active card: the card rises, keeps its size, and
# the cursor ends one gutter above the keys.
present "Keyboard reveal probe"
record reveal-before
before=$(frame "Keyboard reveal probe")
# The gutter above the card is not the card; the card's top rows are.
within "$(band 100 1 1000 8)" 0 0.02
within "$(band 100 14 1000 40)" 0.9 1
raise
record reveal-raised
raised=$(state)
jq -e --argjson b "$before" '
    .trackedFrame.width == $b.width and .trackedFrame.height == $b.height
    and .trackedFrame.x == $b.x and .trackedFrame.y < $b.y
    and ((.cursor.y + .cursor.height + 10 - .panel.y) | fabs) <= 1' <<<"$raised"
echo 'PASS: a covered cursor pans the contents to one gutter above the keys'
# The window rose past the card's top edge, yet nothing of it is drawn there:
# the card stays where it was and its contents slid inside it.
gutter=$(band 100 1 1000 8); inside=$(band 100 14 1000 40)
printf 'reveal-bands gutter=%s inside=%s\n' "$gutter" "$inside"
within "$gutter" 0 0.02
within "$inside" 0.9 1
echo 'PASS: the card frame does not move; only its contents pan'
# The keyboard changes height while it is up. Shorter keys roll nothing back
# down; taller ones cover the line again and roll the contents further up.
# The card never changes size either way.
kwriteconfig6 --notify --file plasmakeyboardrc --group General --key heightPercent 35
sleep 1
record reveal-height-35
jq -e --argjson r "$raised" '
    .panel.y > $r.panel.y and .trackedFrame == $r.trackedFrame
    and .cursor.y + .cursor.height + 10 < .panel.y' <<<"$(state)"
kwriteconfig6 --notify --file plasmakeyboardrc --group General --key heightPercent 55
sleep 1
record reveal-height-55
jq -e --argjson b "$before" --argjson r "$raised" '
    .panel.y < $r.panel.y
    and .trackedFrame.width == $b.width and .trackedFrame.height == $b.height
    and .trackedFrame.x == $b.x and .trackedFrame.y < $r.trackedFrame.y
    and ((.cursor.y + .cursor.height + 10 - .panel.y) | fabs) <= 1' <<<"$(state)"
echo 'PASS: contents roll further up for taller keys and never back down while the keyboard is up'
lower
record reveal-lowered
test "$(frame "Keyboard reveal probe")" = "$before"
echo 'PASS: the Active card returns exactly when the keyboard leaves'

# A field the keyboard never reaches: nothing moves.
present "Keyboard top probe"
before=$(frame "Keyboard top probe")
raise
record top-raised
test "$(frame "Keyboard top probe")" = "$before"
lower
test "$(frame "Keyboard top probe")" = "$before"
echo 'PASS: a visible cursor moves nothing'

# A client that never reports its cursor: nothing moves and nothing breaks.
present "Keyboard blind probe"
before=$(frame "Keyboard blind probe")
raise
record blind-raised
state | jq -e '.visible == true'
test "$(frame "Keyboard blind probe")" = "$before"
lower
test "$(frame "Keyboard blind probe")" = "$before"
echo 'PASS: a window that reports no cursor is never moved'

# Spread: the keyboard over the row changes no window.
kad showCardLine
sleep .6
all=$(probe frames)
raise
record spread-raised
test "$(probe frames)" = "$all"
lower
test "$(probe frames)" = "$all"
echo 'PASS: Spread geometry is untouched by the keyboard'

# Bento is left unpanned: its panes keep their rects, and a covered field
# stays covered. Recorded, not decided.
present "Keyboard reveal probe"
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep 1
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .bentoActive)] | length == 1'
client focusText "Keyboard reveal probe"
sleep .5
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .bentoActive)] | length == 1'
frame "Keyboard reveal probe" | jq -e '.width < 1260'
state | jq -e '.tracked == "Keyboard reveal probe"'
all=$(probe frames)
record bento-before
raise
record bento-raised
test "$(probe frames)" = "$all"
lower
record bento-lowered
test "$(probe frames)" = "$all"
echo 'PASS: Bento geometry is untouched by the keyboard'

test "$(probe releaseRuntime)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
test "$(state | jq '.overlayOption')" = false
echo 'PASS: unloading gives the compositor its own keyboard lift back'
