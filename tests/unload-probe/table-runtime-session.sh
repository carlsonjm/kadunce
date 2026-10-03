#!/usr/bin/env bash
# Table by touch, on the tablet fixture with the tablet kit's direct edges.
# A pull from the top edge opens it; sliding across the tabs previews each
# workspace without switching; lifting on a tab enters it without the slide;
# past the depth line the workspace's cards hang, and lifting on one makes it
# Active there; resting on one never lifts it; back at the edge a lift
# cancels. Pulled past the line under the cards, a card lifts: carried to a
# tab it moves to that workspace, carried to + it makes a workspace named
# after its application. A flick, however far it goes, leaves the tabs open.
# In a menu bar a tab held still, right-clicked or given F2 is renamed: a name
# typed keeps the workspace when empty, a name cleared lets it dissolve, and
# Remove beside a cleared name removes it, its windows going where KDE sends
# them. A scrub resting on a tab never renames it. A tap on
# the preview enters what it shows, and Escape closes Table without changing
# anything. A tap on + makes an empty workspace and enters it; the first
# application in it names it, and left empty it dissolves. The tabs are photographed on screen, and a carried card under the
# finger, for the evidence.
set -Eeuo pipefail
trap 'echo "FAIL: table $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
tp() { qdbus6 org.kde.KWin /TableProof "$@"; }
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
current() { vdm org.kde.KWin.VirtualDesktopManager.current; }
count() { vdm org.kde.KWin.VirtualDesktopManager.count; }
table() { kad tableState | jq -r "$1"; }
# Layout positions, in the whole pixels the probe's touches take.
at() { kad tableState | jq -r "$1 | floor"; }
desktops_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .desktops | join(",")'; }
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
evidence=$(dirname "$XDG_RUNTIME_DIR")
# A photograph of an area of the private displays, kept with the evidence.
shot() { python3 "$(dirname "$0")/capture-png.py" "$1" "$2" "$3" "$4" "$evidence/$5.png"; }
# The tabs' row, above the depth line just under it; the cards' row, above
# the lift line just under it; and past that line.
row() { at '.layout.tabs[0][1]'; }
cards_row() { kad tableState | jq -r '.layout.trayTop + 30 | floor'; }
below_cards() { kad tableState | jq -r '.layout.lift + 30 | floor'; }
# A finger from the top bezel, pulled down far enough to be Table's.
pull() {
    probe down 90 "$1" 4
    for y in 14 28 44 56; do probe motion 90 "$1" "$y"; sleep .02; done
}
# A pull let go before it scrubs: the tabs stay as a menu bar.
flick() {
    probe down 90 "$1" 4
    for y in 14 28 44; do probe motion 90 "$1" "$y"; sleep .02; done
    probe up 90
    sleep .5
}
# A flick the hand carries on past the cards and the line under them, let go
# before any scrub could have been meant.
long_flick() {
    probe down 90 "$1" 4
    for y in 14 28 150 300; do probe motion 90 "$1" "$y"; done
    probe up 90
    sleep .5
}
slide() {
    local x=$1 y=$2
    probe motion 90 "$x" "$y"
    sleep .12
}
lift() { probe up 90; sleep .8; }
tap() { probe down 91 "$1" "$2"; sleep .05; probe up 91; sleep .5; }
escape() { probe key 1 0; sleep .5; }
# A finger held still on a menu bar's tab, then lifted.
hold() { probe down 91 "$1" "$2"; sleep .9; probe up 91; sleep .4; }
# Keys by their evdev codes: letters, Backspace, Enter, F2.
keys() { for code in "$@"; do probe key "$code" 0; sleep .08; done; sleep .3; }
# Slides across the hanging cards until the preview shows the named one,
# leaving the finger's place across in card_x.
onto_card() {
    local caption=$1 x y
    y=$(cards_row)
    for x in $(at '.layout.tray[][0]'); do
        slide "$x" "$y"
        card_x=$x
        [[ $(table '.previewCard') == "$caption" ]] && return 0
    done
    return 1
}
mkdir -p "$XDG_RUNTIME_DIR/z13-tablet-kit"
echo tablet >"$XDG_RUNTIME_DIR/z13-tablet-kit/posture"
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
client colouredCompanion 'One A' c0392b 500 360
client colouredCompanion 'One B' 2980b9 500 360
client colouredCompanion 'Two A' 27ae60 500 360
sleep .8
one=$(current)
vdm createDesktop 1 Two
sleep .3
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
probe sendToDesktop 'Two A' "$two"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
probe activateWindowId "$(id_of 'One A')" >/dev/null
sleep .5
kad workspaceContext | jq -e '.displayContext.edgeBackend == "z13-direct"' >/dev/null
test "$(table '.open')" = false

tp resetWatch
front=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
shot 0 0 1280 200 tabs-closed
pull 640
test "$(table '.open')" = true
# Table is drawn in the compositor's own frames: the tabs are on screen.
sleep .3
shot 0 0 1280 200 tabs-open
if cmp -s "$evidence/tabs-closed.png" "$evidence/tabs-open.png"; then echo 'Table drew nothing' >&2; false; fi
# A scene that fails to load paints its display black: the first tab is lit.
black=$(python3 "$(dirname "$0")/capture-band.py" "$(( $(at '.layout.tabs[0][0]') - 10 ))" "$(( $(row) - 10 ))" 20 20 000000)
python3 -c 'import sys; sys.exit(float(sys.argv[1]) > 0.5)' "$black" || { echo "Table drew black: $black" >&2; false; }
# A pull begun in the gutter above the front card reaches no window.
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$front"
test "$(table '.sticky')" = false
test "$(table '.level')" = tabs
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
test "$(table '.hovered')" = 1
test "$(table '.preview')" = "$two"
test "$(current)" = "$one"
lift
test "$(current)" = "$two"
test "$(table '.open')" = false
test "$(table '.preview')" = ""
test "$(tp watched | jq .slideSeen)" = false
echo 'PASS: a pull from the top edge opens Table, sliding across previews a workspace without switching, and lifting enters it without the slide'

pull 640
slide "$(at '.layout.tabs[0][0]')" "$(row)"
slide "$(at '.layout.tabs[0][0]')" "$(kad tableState | jq -r '.layout.depth + 20 | floor')"
test "$(table '.level')" = cards
test "$(table '.locked')" = 0
onto_card 'One B'
test "$(current)" = "$two"
sleep .8
test "$(table '.carrying')" = false
lift
test "$(current)" = "$one"
kad workspaceContext | jq -e --arg id "$(id_of 'One B')" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id' >/dev/null
echo 'PASS: past the depth line the cards hang, resting on one leaves it chosen, and lifting on one makes it Active in its own workspace'

pull 640
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" 20
test "$(table '.cancelling')" = true
test "$(table '.preview')" = "$one"
lift
test "$(table '.open')" = false
test "$(current)" = "$one"
kad workspaceContext | jq -e --arg id "$(id_of 'One B')" '.cardStage.selectedCardId == $id' >/dev/null
echo 'PASS: pushed back to the edge, a lift cancels and nothing changes'

pull 640
slide "$(at '.layout.tabs[0][0]')" "$(row)"
slide "$(at '.layout.tabs[0][0]')" "$(cards_row)"
onto_card 'One A'
test "$(table '.carrying')" = false
slide "$card_x" "$(below_cards)"
test "$(table '.carrying')" = true
slide 900 360
shot 700 200 400 250 carried-card
slide "$(at '.layout.tabs[1][0]')" 140
slide "$(at '.layout.tabs[1][0]')" "$(row)"
lift
test "$(desktops_of 'One A')" = "$two"
test "$(table '.open')" = true
test "$(table '.sticky')" = true
test "$(table '.locked')" = 1
escape
test "$(table '.open')" = false
test "$(current)" = "$one"
echo "PASS: a card pulled past the line under the cards lifts, and carried to another workspace's tab it moves there"

pull 640
slide "$(at '.layout.tabs[0][0]')" "$(row)"
slide "$(at '.layout.tabs[0][0]')" "$(cards_row)"
onto_card 'One B'
slide "$card_x" "$(below_cards)"
test "$(table '.carrying')" = true
slide "$(at '.layout.plus[0]')" 140
slide "$(at '.layout.plus[0]')" "$(row)"
lift
test "$(count)" = 3
made=$(table '.workspaces[2].id')
test "$(table '.workspaces[2].name')" = "$(table '.workspaces[2].cards[0]')"
test "$(desktops_of 'One B')" = "$made"
escape
test "$(table '.open')" = false
echo 'PASS: carried to +, a card makes a workspace named after its application'

front=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
long_flick 640
test "$(table '.open')" = true
test "$(table '.sticky')" = true
test "$(table '.carrying')" = false
test "$(table '.level')" = tabs
test "$(current)" = "$one"
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$front"
echo 'PASS: a flick carried past the cards and the line under them leaves the tabs open and chooses nothing'
escape

# A pull resting on a tab only looks at it, however long it rests.
pull 640
slide "$(at '.layout.tabs[2][0]')" "$(row)"
sleep .9
test "$(table '.renaming')" = -1
slide "$(at '.layout.tabs[2][0]')" 20
lift
test "$(table '.open')" = false
test "$(current)" = "$one"
echo 'PASS: a scrub resting on a tab never renames it'

flick 640
test "$(table '.open')" = true
test "$(table '.sticky')" = true
hold "$(at '.layout.tabs[2][0]')" "$(row)"
test "$(table '.renaming')" = 2
test "$(table '.open')" = true
test "$(table '.renameText')" = "$(table '.workspaces[2].name')"
# Typing replaces the name, which starts chosen: K with Shift, e, p, t, then
# Enter.
probe key 37 42
keys 18 25 20
test "$(table '.renameText')" = Kept
keys 28
test "$(table '.renaming')" = -1
test "$(table '.workspaces[2].name')" = Kept
table '.named[]' | grep -qx "$made"
kreadconfig6 --file kaduncerc --group Table --key NamedDesktops | grep -q "$made"
qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -q 'Kept'
escape
probe sendToDesktop 'One B' "$one"
sleep .8
test "$(count)" = 3
echo 'PASS: a tab held in a menu bar is renamed, and the name keeps the workspace when empty'

# F2 renames the workspace in view; cleared, the name offers removal, and
# kept cleared it keeps the workspace no longer.
flick 640
keys 106 106
keys 60
test "$(table '.renaming')" = 2
test "$(table '.layout.remove')" = null
keys 14
test -z "$(table '.renameText')"
test "$(table '.layout.remove')" != null
keys 28
test "$(table '.named | length')" = 0
escape
sleep .5
test "$(count)" = 2
test -z "$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o "$made" || true)"
echo 'PASS: F2 renames, and a name cleared lets its empty workspace dissolve'

# A right-click renames too, and Escape leaves the old name.
flick 640
probe pointer "$(at '.layout.tabs[1][0]')" "$(row)"
probe rightClick
sleep .4
test "$(table '.renaming')" = 1
keys 37
escape
test "$(table '.renaming')" = -1
test "$(table '.open')" = true
test "$(table '.workspaces[1].name')" = Two
escape
probe pointer 600 400
echo 'PASS: a right-click renames a tab, and Escape keeps its old name'

flick 640
tap "$(at '.layout.tabs[1][0]')" "$(row)"
test "$(table '.preview')" = "$two"
tap 640 760
test "$(table '.open')" = false
test "$(current)" = "$two"
echo 'PASS: a tap on the preview enters the workspace it shows'

before=$(count)
flick 640
tap "$(at '.layout.plus[0]')" "$(row)"
test "$(table '.open')" = false
test "$(count)" = $((before + 1))
fresh=$(current)
test "$fresh" != "$two"
flick 640
test "$(table '.workspaces[-1].id')" = "$fresh"
test "$(table '.workspaces[-1].current')" = true
test -z "$(table '.workspaces[-1].name')"
escape
echo 'PASS: a tap on + makes an empty workspace and enters it, its tab its number alone'
probe sendToDesktop 'One B' "$fresh"
sleep .8
flick 640
test "$(table '.workspaces[-1].name')" = "$(table '.workspaces[-1].cards[0]')"
escape
probe sendToDesktop 'One B' "$one"
sleep .8
flick 640
tap "$(at '.layout.tabs[1][0]')" "$(row)"
tap "$(at '.layout.tabs[1][0]')" "$(row)"
test "$(current)" = "$two"
sleep .5
test "$(count)" = "$before"
echo 'PASS: the first application in it names it, and left empty it dissolves'

# Remove beside a cleared name removes a workspace with a window in it; KDE
# sends the window to the workspace that takes its place, and Table stays.
flick 640
hold "$(at '.layout.tabs[0][0]')" "$(row)"
test "$(table '.renaming')" = 0
keys 14
tap "$(at '.layout.remove[0]')" "$(at '.layout.remove[1]')"
test "$(count)" = $((before - 1))
test "$(desktops_of 'One B')" = "$two"
test "$(table '.open')" = true
test "$(table '.renaming')" = -1
escape
echo 'PASS: Remove beside a cleared name removes the workspace, its window going where KDE sends it'
test -z "$(kad ownershipViolations)"
echo 'PASS: no window had two owners'
