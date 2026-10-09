#!/usr/bin/env bash
# A stack in Table is one card, its face in front. Carried
# to another workspace's tab it goes whole and stays one card there, and once
# that workspace is shown its windows are one stack again with the same face.
# Needs the tablet fixture and the tablet kit's direct edges.
set -Eeuo pipefail
trap 'echo "FAIL: table stack $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
source "$(dirname "${BASH_SOURCE[0]}")/spread-carry.bash"
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
current() { vdm org.kde.KWin.VirtualDesktopManager.current; }
table() { kad tableState | jq -r "$1"; }
at() { kad tableState | jq -r "$1 | floor"; }
desktops_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .desktops | join(",")'; }
row() { at '.layout.tabs[0][1]'; }
cards_row() { kad tableState | jq -r '.layout.trayTop + 30 | floor'; }
below_cards() { kad tableState | jq -r '.layout.lift + 30 | floor'; }
pull() {
    probe down 90 "$1" 4
    for y in 14 28 44 56; do probe motion 90 "$1" "$y"; sleep .02; done
}
slide() { probe motion 90 "$1" "$2"; sleep .12; }
lift() { probe up 90; sleep .8; }
escape() { probe key 1 0; sleep .5; }
evidence=$(dirname "$XDG_RUNTIME_DIR")
# A photograph of an area of the private displays, kept with the evidence.
shot() { python3 "$(dirname "$0")/capture-png.py" "$1" "$2" "$3" "$4" "$evidence/$5.png"; }
# The stack's two windows by title, as cards of the desktop shown.
stack_members() { kad workspaceContext | jq -c '[.applications[] | select(.hasCard and .stackSize == 2) | .title] | sort'; }
one_stack() {
    kad workspaceContext | jq -e --argjson m "$members" '[.applications[] | select(.hasCard and (.title as $t | $m | index($t)))]
        | length == 2 and ([.[].stackId] | unique | length) == 1 and all(.[]; .stackSize == 2)' >/dev/null
}
mkdir -p "$XDG_RUNTIME_DIR/z13-tablet-kit"
echo tablet >"$XDG_RUNTIME_DIR/z13-tablet-kit/posture"
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client colouredCompanion 'Deck A' c0392b 500 400
client colouredCompanion 'Deck B' 2980b9 500 400
client colouredCompanion 'Two A' 27ae60 500 400
sleep .8
one=$(current)
vdm createDesktop 1 Two
sleep .3
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
probe sendToDesktop 'Two A' "$two"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8

# Two cards form one stack in Spread, as the stack scene forms one.
kad showCardLine
sleep .3
carry_onto_centre 51
probe up 51
sleep .5
members=$(stack_members)
test "$(jq length <<<"$members")" = 2
one_stack
echo 'PASS: two cards form one stack'

pull 640
test "$(table '.open')" = true
slide "$(at '.layout.tabs[0][0]')" "$(row)"
# One card fewer than there are windows, the stack's.
test "$(table '.workspaces[0].cards | length')" = 2
at_deck=$(table '.workspaces[0].stacked | index(1)')
face=$(table ".workspaces[0].titles[$at_deck]")
jq -e --arg f "$face" 'index($f) != null' <<<"$members" >/dev/null
echo 'PASS: in Table the stack is one card, its face in front'

slide "$(at '.layout.tabs[0][0]')" "$(cards_row)"
slide "$(at ".layout.tray[$at_deck][0]")" "$(cards_row)"
sleep .3
shot 0 0 1280 220 deck
slide "$(at ".layout.tray[$at_deck][0]")" "$(below_cards)"
test "$(table '.carrying')" = true
slide "$(at '.layout.tabs[1][0]')" 140
slide "$(at '.layout.tabs[1][0]')" "$(row)"
lift
while read -r title; do test "$(desktops_of "$title")" = "$two"; done < <(jq -r '.[]' <<<"$members")
test "$(current)" = "$one"
kad tableState | jq -e --arg f "$face" '.workspaces[1] | (.cards | length) == 2 and (.stacked | index(1)) != null and (.titles | index($f)) != null' >/dev/null
escape
echo "PASS: carried to another workspace's tab the stack goes whole, and is one card there"

# Shown, that workspace holds the two windows as one stack again.
pull 640
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
lift
test "$(current)" = "$two"
sleep 1.5
one_stack || { echo "DIAG arrived: $(kad workspaceContext | jq -c '[.applications[] | {title, hasCard, stackId, stackSize}]')"; false; }
kad toggleTable
sleep .5
kad tableState | jq -e --arg f "$face" '[.workspaces[] | select(.current)][0] | (.stacked | index(1)) != null and (.titles | index($f)) != null' >/dev/null \
    || { echo "DIAG table: $(kad tableState | jq -c '.workspaces')"; false; }
kad toggleTable
sleep .4
echo 'PASS: once shown, the carried windows are one stack again, the same face in front'
test -z "$(kad ownershipViolations)"
echo 'PASS: no window had two owners'
