#!/usr/bin/env bash
# A card chosen from Spread does not bring the keys with it, even when its
# application used them before (DECISIONS.md § The keyboard comes up for the
# text, not for focus). The tap that chooses a card is Kadunce's own, never a
# tap on the card's text, wherever on the card it lands; and once keys a tap
# brought have been put away, the application taking the focus back into that
# field is not that tap again. A new tap on the text still brings them; a tap
# from one field to another keeps them up, including one a browser answers
# only once the field before has let go and the keys have gone; and a field
# handing the focus to the next keeps the keys and the card's room still.
#
# Three windows: Qt fields at the bottom and halfway down, the second where a
# card's middle lands once it opens, and a GTK field halfway down; GTK asks for
# text input on every focus. Each card is chosen by a tap at the height its
# field will stand, as near as the card reaches.
#
# Needs the tablet fixture: only a display that can own cards presents Spread.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
source "$(dirname "$0")/keyboard-keys.bash"
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: keyboard spread: $name" >&2; failures=$((failures + 1)); fi
}
up() { test "$(vk visible)" = true; }
down() { test "$(vk visible)" = false; }
tap() { probe down 1 "$1" "$2"; sleep .05; probe up 1; }
frame() { probe windowFacts | jq -c --arg t "$1" 'first(.[] | select(.caption == $t)) | {x, y, width, height}'; }
keys_line() { probe keysHistory | jq -c 'map({ms: (.ms | floor), visible, cursorY: .cursor.y, fingerY: .finger.y})'; }
for attempt in {1..40}; do qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe && break; sleep .1; done
for attempt in {1..40}; do [[ $(vk available 2>/dev/null) == true ]] && break; sleep .1; done
test "$(vk available)" = true || { echo 'FAIL: keyboard spread: no input method' >&2; exit 1; }
echo "  mode $(vk mode) (1 is touch only)"
QT_IM_MODULE=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
python3 "$(dirname "$0")/gtk-text.py" --refocus --centre &
gtk_pid=$!
trap 'kill "$client_pid" "$gtk_pid" 2>/dev/null || true' EXIT
sleep 1
client textCompanion
client centreTextCompanion
sleep 1.5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1

# Opens Spread and taps the named card at height `y`, as near as the card
# reaches, stepping the row until the card stands on the display.
choose() {
    local title=$1 y=$2 rect step
    kad showCardLine
    sleep .6
    spread_rect() { kad workspaceContext | jq -c --arg t "$title" 'first(.applications[] | select(.title == $t)) | .spreadRect'; }
    for step in 1 2 3; do
        jq -e '. != null and .x >= 0 and .x + .width <= 1280' <<<"$(spread_rect)" >/dev/null && break
        if (($(jq '.x // -1 | floor' <<<"$(spread_rect)") < 0)); then probe key 105 0; else probe key 106 0; fi
        sleep .6
    done
    rect=$(spread_rect)
    # A height below zero counts from the card's top edge.
    read -r x y < <(jq -r --argjson y "$y" '[.x + .width / 2, (if $y < 0 then .y - $y else ([.y + 10, ([.y + .height - 10, $y] | min)] | max) end)] | map(floor) | @tsv' <<<"$rect")
    probe watchKeys
    tap "$x" "$y"
    sleep 1.5
    echo "  chose $title at $rect by a tap at $x $y: keys $(vk visible) $(kad workspaceContext | jq -c '{presentation: .cardStage.presentation, focus: .focus.title}')"
    echo "    shown/hidden $(keys_line)"
}
refocus() {
    probe watchKeys
    case $1 in
        'Keyboard GTK centre probe') kill -USR1 "$gtk_pid"; sleep .5 ;;
        *) client focusText "$1" ;;
    esac
    sleep 1.5
    echo "  the application focuses its field: keys $(vk visible)"
    echo "    shown/hidden $(keys_line)"
}
put_away() {
    local x y
    read -r x y < <(keyboard_key_point hide "$(probe keyboardState | jq -c '.panel')")
    tap "$x" "$y"
    sleep 1.5
}
# Opens the window as the Active card and taps its field; prints the line the
# text cursor stands on.
tap_text() {
    local title=$1 cursor x y
    cursor=$(probe keyboardState | jq -c '.cursor')
    x=$(jq '.x + 40 | floor' <<<"$cursor"); y=$(jq '.y + .height / 2 | floor' <<<"$cursor")
    probe watchKeys
    tap "$x" "$y"
    sleep 1.5
    echo "  a tap on the text at $x $y: keys $(vk visible)" >&2
    echo "    shown/hidden $(keys_line)" >&2
    echo "$y"
}

for title in 'Keyboard reveal probe' 'Keyboard centre probe' 'Keyboard GTK centre probe'; do
    echo "== $title"
    choose "$title" 400
    check "$title: chosen the first time, the keys stay down" down
    refocus "$title"
    line=$(tap_text "$title")
    check "$title: a tap on its text brings the keys up" up
    put_away
    check "$title: the Hide key puts them away" down
    # Another card in between, so the window has the focus to take back.
    other='Keyboard reveal probe'; [[ $title == "$other" ]] && other='Keyboard centre probe'
    choose "$other" 400
    choose "$title" "$line"
    check "$title: chosen again from Spread by a tap where its text will be, the keys stay down" down
    refocus "$title"
    check "$title: the application focusing its field on its own leaves them down" down
    probe hideKeyboard
    sleep .6
done

# Without Spread: the keys put away by their Hide key, and the application
# focusing its field again. The last touch was on the keys.
title='Keyboard centre probe'
echo "== $title, without Spread"
test "$(kad activateApplicationWindow "$(probe windowIdByCaption "$title")")" = true
sleep 1
refocus "$title"
line=$(tap_text "$title")
check 'a tap on the text brings the keys up again' up
put_away
check 'the Hide key puts them away again' down
refocus "$title"
check 'the application focusing its field after the Hide key leaves them down' down

# Typing ends with the field let go rather than the keys put away, then the
# card is chosen again and its application focuses the field once more.
echo "== $title, the field let go"
line=$(tap_text "$title")
check 'a tap on the text brings the keys up again' up
client leaveText "$title"
sleep 1.5
check 'the field let go, the keys go down' down
choose 'Keyboard reveal probe' 400
choose "$title" "$line"
check 'chosen from Spread with its field let go, the keys stay down' down
refocus "$title"
check 'the application focusing its field after Spread leaves them down' down
probe hideKeyboard
sleep .6
client focusText "$title"
sleep 1.5
line=$(tap_text "$title")
check 'and a tap on the text still brings them up' up

# From one field to another with the keys up: they stay up. The GTK window
# holds a second field at its top edge; GTK asks for text input on every
# focus, where Qt asks only when a field already focused is tapped.
echo "== from field to field"
gtk='Keyboard GTK centre probe'
test "$(kad activateApplicationWindow "$(probe windowIdByCaption "$gtk")")" = true
sleep 1
refocus "$gtk"
line=$(tap_text "$gtk")
check 'a tap on the GTK field brings the keys up' up
probe watchKeys
gframe=$(frame "$gtk")
read -r x y < <(jq -r '[.x + 40, .y + 17] | map(floor) | @tsv' <<<"$gframe")
tap "$x" "$y"
sleep 1.5
echo "  the top field at $x $y of $gframe: keys $(vk visible) cursor $(probe keyboardState | jq -c '.cursor')"
echo "    shown/hidden $(keys_line)"
check 'a tap on its other field, which takes the focus as a browser page does, brings them back up' up
check 'and the text cursor moved into that field' jq -e --argjson f "$gframe" '.y < $f.y + 40' <<<"$(probe keyboardState | jq -c '.cursor')"
probe watchKeys
tap 59 "$line"
sleep 1.5
echo "  back to the middle field at 59 $line: keys $(vk visible) cursor $(probe keyboardState | jq -c '.cursor')"
echo "    shown/hidden $(keys_line)"
check 'a tap back on the first field keeps them up' up
sleep .5
echo "  after the hop: card $(frame "$gtk") keys $(probe keyboardState | jq -c '.panel')"
check 'and the card keeps the room above the keys' jq -e --argjson p "$(probe keyboardState | jq -c '.panel')" '.y + .height <= $p.y' <<<"$(frame "$gtk")"
probe watchPanel
kill -USR2 "$gtk_pid"
sleep 1.5
echo "  handed between fields: keys $(vk visible) card $(frame "$gtk") panel heights $(probe panelHistory) cursor $(probe keyboardState | jq -c '.cursor')"
check 'handed from field to field, the keys stay up' up
check 'and the card keeps the room above them' jq -e --argjson p "$(probe keyboardState | jq -c '.panel')" '.y + .height <= $p.y' <<<"$(frame "$gtk")"
probe hideKeyboard
sleep 1

test "$(probe releaseRuntime)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: keyboard spread: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card chosen from Spread, and a field its application focuses, leave the keys down; taps on the text bring them'
