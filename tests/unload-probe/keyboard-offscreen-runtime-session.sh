#!/usr/bin/env bash
# Keys the compositor shows while none of them is on screen do not count as
# up (DECISIONS.md § The dock steps aside for the keys, § The Active card
# makes room for the keys). A stand-in Bottom Surface holds a 64 px band along
# the tablet's bottom edge and steps aside when the Keyboard asks for the
# region, and the Keyboard holds its keys below the screen's edge until the
# band has let its room go. While keys are held off screen the Active card
# makes no room for them and never grows onto the band. An application taking
# the focus back into its own field, with nobody touching it, is not asking
# for the keys (§ The keyboard comes up for the text, not for focus): they
# stay down and the band keeps its room, however often it happens.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: keyboard offscreen: $name" >&2; failures=$((failures + 1)); fi
}
up() { test "$(vk visible)" = true; }
down() { test "$(vk visible)" = false; }
tap() { probe down 1 "$1" "$2"; sleep .05; probe up 1; }
frame() { probe windowFacts | jq -c --arg t "$1" 'first(.[] | select(.caption == $t)) | {x, y, width, height}'; }
# The Active card on a 1280x800 tablet above the 64 px band: its bottom edge
# a gutter above the band's top edge at 736.
band_top=736
docked='{"x":10,"y":10,"width":1260,"height":716}'
on_band() { jq -e --argjson top "$band_top" '.y + .height > $top - 10' <<<"$1"; }
# The frames the band watch recorded, read for what decides the checks: how
# many, the frames where the compositor showed keys none of which were on
# screen (the panel is then the Keyboard's two-pixel strip), the card's least
# height in those and where the strip stood, and the lowest the card's bottom
# edge reached in any frame.
summary() {
    jq -c '{frames: length,
        heldOffScreen: (map(select(.shown and .keysHeight <= 2)) | length),
        heldFrom: (map(select(.shown and .keysHeight <= 2)) | first.ms // null | if . then floor else . end),
        heldTo: (map(select(.shown and .keysHeight <= 2)) | last.ms // null | if . then floor else . end),
        cardHeightWhileHeld: (map(select(.shown and .keysHeight <= 2) | .cardHeight) | min),
        heldKeysTopAndCard: (map(select(.shown and .keysHeight <= 2) | "\(.keysTop)/\(.cardHeight)") | unique),
        lowestCardBottom: (map(.cardBottom) | max)}' <<<"$1"
}
no_room_while_held() { jq -e --argjson d "$docked" 'all(.[] | select(.shown and .keysHeight <= 2); .cardHeight >= $d.height)' <<<"$1"; }
never_on_band() { jq -e --argjson top "$band_top" 'all(.[]; (.cardBottom // 0) <= $top - 10)' <<<"$1"; }
keys_line() { jq -c 'map({ms: (.ms | floor), visible, cursorY: .cursor.y, fingerY: .finger.y, panel: .panel.height})' <<<"$(probe keysHistory)"; }
for attempt in {1..40}; do qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe && break; sleep .1; done
for attempt in {1..40}; do [[ $(vk available 2>/dev/null) == true ]] && break; sleep .1; done
test "$(vk available)" = true || { echo 'FAIL: keyboard offscreen: no input method' >&2; exit 1; }
QT_IM_MODULE=wayland "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
python3 "$(dirname "$0")/gtk-text.py" --refocus &
gtk_pid=$!
trap 'kill "$client_pid" "$gtk_pid" 2>/dev/null || true' EXIT
sleep 1
client bottomSurface 64
client textCompanion
sleep 1.5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1

present() {
    local title=$1
    kad showCardLine
    sleep .4
    client focusText "$title"
    sleep .4
    kad showActive
    sleep 1
}

# The keys a tap earns wait below the edge until the band has stepped aside,
# then rise; the card takes room only from keys on screen.
reveal='Keyboard reveal probe'
present "$reveal"
echo "  band $(client bandState) card $(frame "$reveal") work area $(probe keyboardState | jq -c '.workArea')"
check 'the Active card stops at the band' test "$(frame "$reveal")" = "$docked"
read -r lx ly < <(client fieldCentre "$reveal" revealField)
card=$(frame "$reveal")
fx=$(($(jq '.x' <<<"$card") + lx)); fy=$(($(jq '.y' <<<"$card") + ly))
probe watchKeys
probe watchBand "$reveal"
tap "$fx" "$fy"
sleep 1.5
history=$(probe bandHistory)
state=$(probe keyboardState)
echo "  a tap on the field at $fx $fy: keys $(vk visible) band $(client bandState) $(jq -c '{panel, trackedFrame, workArea}' <<<"$state")"
echo "    frames $(summary "$history")"
echo "    shown/hidden $(keys_line)"
check 'a tap on the field brings the keys up' up
check 'the Keyboard asked the band for its room, and the band stepped aside' \
    jq -e '.yields >= 1 and (.reserving | not)' <<<"$(client bandState)"
check 'keys held off screen take no room from the card' no_room_while_held "$history"
check 'the card never grows onto the band while the keys arrive' never_on_band "$history"
check 'at rest the card ends a gutter above the keys on screen' jq -e '.panel.height > 2
    and .trackedFrame.y == 10 and ((.trackedFrame.y + .trackedFrame.height + 10 - .panel.y) | fabs) <= 1' <<<"$state"

# Typing ends: the keys slide out, the Keyboard lets the region go, the band
# takes its room back, and the card stops at the band again.
probe watchBand "$reveal"
client leaveText "$reveal"
sleep 1.5
history=$(probe bandHistory)
echo "  typing ends: keys $(vk visible) band $(client bandState) card $(frame "$reveal")"
echo "    frames $(summary "$history")"
band_back() { down && jq -e '.reserving' <<<"$(client bandState)"; }
check 'the keys gone, the band has its room back and the card stops at it' \
    eval 'band_back && test "$(frame "$reveal")" = "$docked"'
check 'the card never grows onto the band as the keys leave' never_on_band "$history"

# An application that takes the focus back into its field, with nobody
# touching anything since the person last typed there. GTK asks for text input
# on every focus, so the compositor shows keys each time it does.
gtk='Keyboard GTK probe'
kad showCardLine
sleep .5
spread_rect() { kad workspaceContext | jq -c --arg t "$gtk" 'first(.applications[] | select(.title == $t)) | .spreadRect'; }
# The row moves one card at a time until the GTK card stands on the display.
for step in 1 2; do
    jq -e '. != null and .x >= 0 and .x + .width <= 1280' <<<"$(spread_rect)" >/dev/null && break
    if (($(jq '.x // -1 | floor' <<<"$(spread_rect)") < 0)); then probe key 105 0; else probe key 106 0; fi
    sleep .6
done
rect=$(spread_rect)
echo "  in Spread, the GTK card at $rect"
tap "$(jq '.x + .width / 2 | floor' <<<"$rect")" "$(jq '.y + .height / 2 | floor' <<<"$rect")"
sleep 1.2
card=$(frame "$gtk")
echo "  the GTK card chosen: keys $(vk visible) card $card $(kad workspaceContext | jq -c '{presentation: .cardStage.presentation, focus: .focus.title}')"
check 'the GTK card is Active and stops at the band' \
    eval '! on_band "$card" && test "$(kad workspaceContext | jq -r .focus.title)" = "$gtk"'
cursor=$(probe keyboardState | jq -c '.cursor')
gx=$(jq '.x + 40 | floor' <<<"$cursor"); gy=$(jq '.y + .height / 2 | floor' <<<"$cursor")
probe watchKeys
tap "$gx" "$gy"
sleep 1.5
echo "  a tap on the GTK field at $gx $gy: keys $(vk visible) band $(client bandState) $(probe keyboardState | jq -c '{panel, trackedFrame}')"
echo "    shown/hidden $(keys_line)"
check 'a tap on the GTK field brings the keys up' up
# The application puts the keys away, as one does when typing is done.
probe hideKeyboard
sleep 1.5
echo "  put away: keys $(vk visible) band $(client bandState) card $(frame "$gtk")"
check 'put away, the keys are down and the band has its room back' band_back

probe watchKeys
probe watchBand "$gtk"
samples=()
for cycle in 1 2 3 4; do
    kill -USR1 "$gtk_pid"
    sleep 1.5
    sample=$(jq -c --arg keys "$(vk visible)" --argjson band "$(client bandState)" --argjson card "$(frame "$gtk")" \
        '{keys: ($keys == "true"), keysHeight: .panel.height, band: $band.reserving, card: $card}' <<<"$(probe keyboardState)")
    echo "  the application takes the focus back, $cycle: $sample"
    samples+=("$sample")
done
history=$(probe bandHistory)
all=$(printf '%s\n' "${samples[@]}" | jq -s -c '.')
echo "    frames $(summary "$history")"
echo "    shown/hidden $(keys_line)"
check 'the application taking the focus back, with nobody touching it, leaves the keys down' \
    jq -e 'all(.[]; .keys | not)' <<<"$all"
check 'no keys are left shown with none of them on screen' \
    jq -e 'all(.[]; (.keys and .keysHeight <= 2) | not)' <<<"$all"
check 'the band keeps its room' jq -e 'all(.[]; .band)' <<<"$all"
check 'keys held off screen take no room from the card' no_room_while_held "$history"
check 'the card never grows onto the band' never_on_band "$history"

test "$(probe releaseRuntime)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: keyboard offscreen: $failures checks failed" >&2; exit 1; fi
echo 'PASS: keys held off screen do not count as up, and the card never grows onto the band'
