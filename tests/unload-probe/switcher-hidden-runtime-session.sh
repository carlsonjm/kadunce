#!/usr/bin/env bash
# CARD-LIFECYCLE.md §4: a window explicitly excluded from normal task switching
# never becomes a card. The one here is KWin's own: it has no caption and is
# marked to skip the taskbar and the switcher. Kadunce neither adopts it when
# switched on nor takes it as an arrival, and leaves it where it stands, at its
# own size. A window Kadunce does not hold is painted wherever it stands (§2,
# Native desktop). KWin keeps its own windows above every application window,
# so this one shows over the Active card, and a photograph finds it there; the
# rest of the Active card is drawn as before.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: switcher hidden: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
# KWin's own captionless window, as KWin describes it.
hidden() { probe windowFacts | jq -c 'first(.[] | select(.internal and .caption == "")) // empty'; }
band() { python3 "$(dirname "$0")/capture-band.py" "$@"; }
more() { awk -v v="$1" -v m="$2" 'BEGIN { exit !(v > m) }'; }
report() {
    echo "state $1 $(kad workspaceContext 2>/dev/null | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId, apps: [.applications[] | {title, windowId, hasCard}]}' 2>/dev/null)"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.normal) | {caption, internal, skipSwitcher, skipTaskbar, hidden, x, y, width, height})')"
}
swatch=9b30d0
card=1e90c8
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
# KWin opens a window on the display under the pointer.
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client colouredCompanion 'Card probe' "$card" 700 500
sleep .8
# Near the tablet's left edge, clear of where the photographs of the Active
# card look.
wx=40 wy=300 ww=120 wh=160
probe openSwitcherHiddenWindow "$wx" "$wy" "$ww" "$wh" "$swatch"
sleep 1
before=$(hidden)
report before
echo "switcher hidden window before $before"
check 'the window is KWin own, captionless and hidden from the taskbar and switcher' \
    jq -e '.internal and .caption == "" and .normal and .skipSwitcher and .skipTaskbar' <<<"$before"
check 'it stands where it was put' jq -e --argjson x "$wx" --argjson y "$wy" '.x == $x and .y == $y' <<<"$before"
drawn=$(band "$wx" "$wy" "$ww" "$wh" "$swatch")
echo "before Kadunce: its place shows $drawn of its colour"
check 'before Kadunce, it is painted where it stands' more "$drawn" 0.9
id=$(jq -r '.id' <<<"$before")
place() { hidden | jq -c '{x, y, width, height}'; }
home=$(place)

# §3: switched on, Kadunce adopts every eligible window on the tablet, and
# this one is not eligible.
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
report switched-on
check 'switched on, it is not a card' context --arg id "$id" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 0'
check 'switched on, the two application windows are cards' context '[.applications[] | select(.hasCard)] | length == 2'
check 'switched on, the Active card is an application window' context --arg id "$id" \
    '.cardStage.presentation == "active" and .cardStage.selectedCardId != $id'
echo "switched on: its place and size $(place), before $home"
check 'switched on, Kadunce leaves its place and size' test "$(place)" = "$home"
drawn=$(band "$wx" "$wy" "$ww" "$wh" "$swatch")
echo "switched on: its place shows $drawn of its colour"
check 'switched on, it is painted where it stands' more "$drawn" 0.9
read -r cx cy cw ch < <(probe windowFacts | jq -r 'first(.[] | select(.caption == "Card probe")) | "\(.x) \(.y) \(.width) \(.height)"')
shown=$(band "$((cx + cw / 4))" "$((cy + ch / 4))" "$((cw / 2))" "$((ch / 2))" "$card")
echo "switched on: the Active card at $cx $cy ${cw}x$ch shows $shown of its colour"
check 'switched on, the Active card is drawn' more "$shown" 0.9

# §8: a window that opens while a card is Active becomes the Active card,
# unless it is not eligible at all.
active=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
probe closeSwitcherHiddenWindows
sleep .6
probe openSwitcherHiddenWindow "$wx" "$wy" "$ww" "$wh" "$swatch"
sleep 1.2
report arrived
arrival=$(hidden | jq -r '.id // empty')
echo "arrival $arrival"
check 'it opens while a card is Active' test -n "$arrival"
check 'opened while a card is Active, it is not a card' \
    context --arg id "$arrival" '[.applications[] | select(.windowId == $id and .hasCard)] | length == 0'
check 'opened while a card is Active, the Active card stays' \
    context --arg id "$active" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
echo "arrived: its place and size $(place)"
check 'opened while a card is Active, it keeps its place and size' test "$(place)" = "$home"
drawn=$(band "$wx" "$wy" "$ww" "$wh" "$swatch")
echo "arrived: its place shows $drawn of its colour"
check 'opened while a card is Active, it is painted where it stands' more "$drawn" 0.9
shown=$(band "$((cx + cw / 4))" "$((cy + ch / 4))" "$((cw / 2))" "$((ch / 2))" "$card")
echo "arrived: the Active card at $cx $cy ${cw}x$ch shows $shown of its colour"
check 'opened while a card is Active, the Active card is drawn' more "$shown" 0.9

# §2: Spread shows every individual card, and this window is none.
kad showCardLine
sleep .8
report spread
check 'Spread holds the two application cards and nothing else' \
    context '.cardStage.presentation == "cardLine" and ([.applications[] | select(.hasCard)] | length == 2)'
check 'Spread draws no card for it' context --arg id "$arrival" '[.applications[] | select(.windowId == $id and .spreadRect)] | length == 0'
echo "in Spread: its place shows $(band "$wx" "$wy" "$ww" "$wh" "$swatch") of its colour"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report switched-off
echo "switched off: its place and size $(place)"
check 'switched off, it stands where it stood' test "$(place)" = "$home"
probe closeSwitcherHiddenWindows

if ((failures)); then echo "FAIL: switcher hidden: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a window hidden from the task switcher is never a card and stays painted where it stands'
