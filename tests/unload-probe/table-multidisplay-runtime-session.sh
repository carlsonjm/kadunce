#!/usr/bin/env bash
# Table's preview across the tablet and a monitor. Each desktop has a front
# card and a full-size card behind it on the tablet, and its own layout on the
# monitor, each window in its own colour. Previewing the other desktop draws on
# both displays exactly what that desktop shows, its front card and its panes
# and never a card behind, and nothing of the current desktop; ending the
# preview changes nothing; committing it switches without the slide, while
# KDE's own switch still slides. No window moves at any step, so every card and
# every layout stays where it was, and a card behind another desktop's front
# card is never drawn, even where it reaches past it.
set -Eeuo pipefail
trap 'echo "FAIL: table multidisplay $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
tp() { qdbus6 org.kde.KWin /TableProof "$@"; }
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
current() { vdm org.kde.KWin.VirtualDesktopManager.current; }
# A slide still running from an earlier switch is not the one being watched.
watch_from_rest() {
    for attempt in {1..50}; do [[ $(tp slideActive) == false ]] && break; sleep .1; done
    tp resetWatch
}
switch() { qdbus6 org.kde.KWin /VirtualDesktopManager org.freedesktop.DBus.Properties.Set org.kde.KWin.VirtualDesktopManager current "$1"; sleep .8; }
evidence=$(dirname "$XDG_RUNTIME_DIR")
# The tablet's middle, its left gutter, and the middle of each half of the
# monitor, photographed once per look.
colours=(c0392b 7f8c8d 2980b9 16a085 f39c12 8e44ad 27ae60 d35400)
look() {
    python3 "$(dirname "$0")/capture-shares.py" front=540,350,200,100 gutter=0,100,8,600 \
        left=1500,350,200,100 right=2140,350,200,100 -- "${colours[@]}"
}
# The front card fills the tablet's middle, the layout's two panes fill the
# monitor's halves on either side, and no other window's colour shows anywhere.
shows() {
    local view
    view=$(look)
    echo "look $view"
    jq -e --arg f "$1" --arg a "$2" --arg b "$3" '
        .front[$f] >= 0.9
        and ((.left[$a] >= 0.9 and .right[$b] >= 0.9) or (.left[$b] >= 0.9 and .right[$a] >= 0.9))
        and ([.[] | to_entries[] | select(.key != $f and .key != $a and .key != $b) | .value] | max) <= 0.02
    ' <<<"$view" >/dev/null
}
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
places() { probe windowFacts | jq -c '[.[] | select(.class == "unload-client") | {caption, x, y, width, height}] | sort_by(.caption)'; }
shows_one() { shows c0392b 2980b9 16a085; }
shows_two() { shows f39c12 27ae60 d35400; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_table_proof >/dev/null
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client colouredCompanion 'One behind' 7f8c8d 1280 800
client colouredCompanion 'One front' c0392b 500 360
probe pointer 1900 400
client colouredCompanion 'One left' 2980b9 440 500
client colouredCompanion 'One right' 16a085 440 500
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
probe activateWindowId "$(id_of 'One front')" >/dev/null
sleep .6
test "$(kad toggleBentoOnOutput Virtual-1)" = true
sleep .8
kad outputStageState | rg -q '^Virtual-1\|external\|.*\|2$'
one=$(current)
vdm createDesktop 1 Two
sleep .3
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
switch "$two"
probe pointer 600 400
client colouredCompanion 'Two behind' 8e44ad 1280 800
sleep .8
client colouredCompanion 'Two front' f39c12 500 360
sleep .8
probe pointer 1900 400
client colouredCompanion 'Two left' 27ae60 440 500
client colouredCompanion 'Two right' d35400 440 500
sleep .8
test "$(kad toggleBentoOnOutput Virtual-1)" = true
sleep .8
kad outputStageState | rg -q '^Virtual-1\|external\|.*\|2$'
kad workspaceContext | jq -e --arg id "$(id_of 'Two front')" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id' >/dev/null
shows_two
switch "$one"
shows_one
before=$(places)
echo 'PASS: each desktop has a front card on the tablet and its own layout on the monitor'

# In Table a layout's panes are cards of their own beside the tablet's cards,
# and opening it moves nothing.
probe pointer 600 400
kad toggleTable
sleep .5
kad tableState | jq -e '.open and all(.workspaces[0].stacked[]; . == 0)
    and ([.workspaces[0].titles[] | select(. == "One left" or . == "One right")] | length) == 2' >/dev/null \
    || { echo "DIAG table: $(kad tableState | jq -c '{open, workspaces}')"; false; }
kad toggleTable
sleep .5
test "$(kad tableState | jq -r .open)" = false
test "$(places)" = "$before"
echo "PASS: in Table a layout's panes are cards of their own"

watch_from_rest
test "$(kad previewDesktop "$two")" = true
sleep .4
test "$(current)" = "$one"
shows_two
test "$(places)" = "$before"
echo 'PASS: previewing draws the other desktop on both displays as it stands, and nothing of the current one'
test "$(kad previewDesktop '')" = true
sleep .4
test "$(current)" = "$one"
shows_one
test "$(places)" = "$before"
watched=$(tp watched | jq -c .)
echo "watch $watched"
test "$watched" = '{"desktopChanges":0,"slideSeen":false}'
kad outputStageState | rg -q '^Virtual-1\|external\|.*\|2$'
echo 'PASS: ending the preview changes nothing'

test "$(kad previewDesktop "$two")" = true
sleep .4
test "$(kad commitDesktopPreview)" = true
sleep 1
test "$(current)" = "$two"
test "$(tp watched | jq .slideSeen)" = false
shows_two
test "$(places)" = "$before"
kad outputStageState | rg -q '^Virtual-1\|external\|.*\|2$'
kad workspaceContext | jq -e --arg id "$(id_of 'Two front')" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id' >/dev/null
echo 'PASS: committing switches without the slide, and every card and layout stays where it was'

# The first desktop's card behind never stood in front, yet it stands in the
# Active card's place (CARD-LIFECYCLE.md §3), under the front card.
probe windowFacts | jq -e 'first(.[] | select(.caption == "One behind")) | .x >= 10 and .width <= 1260' >/dev/null
test "$(kad previewDesktop "$one")" = true
sleep .4
test "$(current)" = "$two"
shows_one
test "$(kad previewDesktop '')" = true
sleep .4
shows_two
test "$(places)" = "$before"
echo "PASS: a preview never draws a card that stands behind the other desktop's front card"

watch_from_rest
switch "$one"
sleep .4
test "$(tp watched | jq .slideSeen)" = true
shows_one
test "$(places)" = "$before"
test -z "$(kad ownershipViolations)"
echo "PASS: KDE's own switch still slides and finds the first desktop as it was, and no window had two owners"
