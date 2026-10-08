#!/usr/bin/env bash
# A stack Table carries to a desktop whose tablet holds only a layout waits
# behind the layout as the same stack, and the layout stays as it was: the
# touches on its panes reach them. The stack's windows had lain over the
# panes as windows no one owned, one of them covering the screen, taking every
# touch. Needs the tablet fixture and its direct edges.
set -Eeuo pipefail
trap 'echo "FAIL: table stack layout $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
source "$(dirname "${BASH_SOURCE[0]}")/spread-carry.bash"
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
current() { vdm org.kde.KWin.VirtualDesktopManager.current; }
table() { kad tableState | jq -r "$1"; }
at() { kad tableState | jq -r "$1 | floor"; }
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
row() { at '.layout.tabs[0][1]'; }
cards_row() { kad tableState | jq -r '.layout.trayTop + 30 | floor'; }
below_cards() { kad tableState | jq -r '.layout.lift + 30 | floor'; }
pull() { probe down 90 "$1" 4; for y in 14 28 44 56; do probe motion 90 "$1" "$y"; sleep .02; done; }
slide() { probe motion 90 "$1" "$2"; sleep .12; }
lift() { probe up 90; sleep .8; }
escape() { probe key 1 0; sleep .5; }
# What the probe prints when a check fails.
state() {
    echo "STATE $1: desktop=$(current | cut -c1-8) table=$(kad tableState | jq -c '{open, sticky, keys, level, carrying, workspaces: [.workspaces[] | {current, titles, stacked}]}')"
    echo "   stage=$(kad outputStageState | tr '\n' ' ') intercepted=$(probe mouseIntercepted)"
    echo "   cards=$(kad workspaceContext | jq -c '{p: .cardStage.presentation, active: .cardStage.active, cards: [.applications[] | select(.hasCard) | {t: .title, n: .stackSize}]}')"
    echo "   windows=$(probe windowFacts | jq -c '[.[] | select(.class == "unload-client") | {c: .caption, d: (.desktops | map(.[0:8]) | join(",")), x, y, w: .width, h: .height, min: .minimized}]')"
    echo "   violations=$(kad ownershipViolations | tr '\n' ' ')"
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
client colouredCompanion 'Pane A' c0392b 500 400
client colouredCompanion 'Pane B' 2980b9 500 400
sleep .8
one=$(current)
vdm createDesktop 1 Two
sleep .3
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
switch() { qdbus6 org.kde.KWin /VirtualDesktopManager org.freedesktop.DBus.Properties.Set org.kde.KWin.VirtualDesktopManager current "$1"; sleep .8; }
probe sendToDesktop 'unload-client' "$two"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
# Workspace one holds a tablet layout of its two windows and nothing else.
kad showActive
sleep .4
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep 1
kad outputStageState | rg -q '^Virtual-0\|tablet\|.*\|2$'
kad workspaceContext | jq -e '.cardStage.active | not' >/dev/null
panes=$(probe windowFacts | jq -c '[.[] | select(.caption == "Pane A" or .caption == "Pane B") | {caption, x, y, width, height}] | sort_by(.caption)')
# Workspace two holds a stack of two cards.
switch "$two"
client colouredCompanion 'Solo' 27ae60 500 400
sleep 1.2
kad showCardLine
sleep .4
carry_onto_centre 51; probe up 51; sleep .6
members=$(kad workspaceContext | jq -c '[.applications[] | select(.hasCard and .stackSize == 2) | .title] | sort')
test "$(jq length <<<"$members")" = 2 || { state stack; false; }
echo 'PASS: one workspace holds only a layout, the other a stack'

pull 640
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" "$(cards_row)"
deck=$(table '.workspaces[1].stacked | index(1)')
slide "$(at ".layout.tray[$deck][0]")" "$(cards_row)"
slide "$(at ".layout.tray[$deck][0]")" "$(below_cards)"
test "$(table '.carrying')" = true
slide "$(at '.layout.tabs[0][0]')" 140
slide "$(at '.layout.tabs[0][0]')" "$(row)"
lift
escape
pull 640
slide "$(at '.layout.tabs[0][0]')" "$(row)"
slide "$(at '.layout.tabs[0][0]')" "$(row)"
lift
sleep 2
test "$(current)" = "$one"
kad outputStageState | rg -q '^Virtual-0\|tablet\|.*\|2$' || { state layout; false; }
test "$(probe windowFacts | jq -c '[.[] | select(.caption == "Pane A" or .caption == "Pane B") | {caption, x, y, width, height}] | sort_by(.caption)')" = "$panes"
kad workspaceContext | jq -e --argjson m "$members" '.cardStage.presentation == "bento"
    and ([.applications[] | select(.hasCard and (.title as $t | $m | index($t)))] as $c
         | ($c | length) == 2 and ([$c[].stackId] | unique | length) == 1 and all($c[]; .stackSize == 2))' >/dev/null \
    || { state arrived; false; }
echo 'PASS: carried to a workspace holding only a layout, the stack waits behind it as the same stack, and the layout stays'
for point in "320 400" "960 400" "640 120"; do
    probe windowAt $point | jq -e '.target.caption == "Pane A" or .target.caption == "Pane B"' >/dev/null \
        || { echo "DIAG at $point: $(probe windowAt $point | jq -c .target)"; false; }
done
echo "PASS: the layout's panes take the touches on them, not the windows behind"
test -z "$(kad ownershipViolations)"
echo 'PASS: no window had two owners'
