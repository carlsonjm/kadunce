#!/usr/bin/env bash
# INPUT.md § Connected, by the keys, as a desk is set up: Virtual-0 is a
# small display left of Virtual-1, holds no window, and Virtual-1 is large,
# sized Connected and chosen for Spread, with two windows paired there. A
# pane Meta+Shift+Left carries onto the small display is the card shown there,
# and Meta+Shift+Right brings it back as a card. A Stack carried there steps
# by Meta+Tab and comes back whole. Meta+B then lays out both again, and
# Spread shows them as two cards, not one Stack. No step refocuses a window
# between keys, as a person at the keyboard does not. Every check is
# reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: desk keys: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, output, hasCard, pane, paneStack, stackId, focused, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden})')"
}
spread_key() { probe key 31 125 >/dev/null; sleep 1.2; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.4; }
selected() { [[ $(kad workspaceContext | jq -r '.cardStage.selectedCardId') == "$1" ]]; }
select_card() {
    local id=$1 key
    for key in 105 106; do
        for _ in {1..8}; do
            selected "$id" && return 0
            probe key "$key" 0 >/dev/null
            sleep .35
        done
    done
    selected "$id"
}
pick() { select_card "$1" || echo "could not select $1"; open_selected; }
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
app() { kad workspaceContext | jq -c --arg id "$1" 'first(.applications[] | select(.windowId == $id))'; }
focused_id() { kad workspaceContext | jq -r 'first(.applications[] | select(.focused)) | .windowId'; }
focused() { [[ $(focused_id) == "$1" ]]; }
output_of() { kad workspaceContext | jq -r --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .output'; }
on_output() { [[ $(output_of "$1") == "$2" ]]; }
is_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .pane and (.hasCard | not) and (.minimized | not)'; }
is_kept() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .pane or .hasCard'; }
shown_on() { probe windowFacts | jq -e --arg id "$1" --arg o "$2" 'first(.[] | select(.id == $id)) | .output == $o and (.hidden | not)'; }
large_panes() { kad workspaceContext | jq -r '[.applications[] | select(.pane and (.hasCard | not) and .output == "Virtual-1") | .windowId] | .[]'; }
pane_count() { large_panes | wc -l; }
together() { context --arg a "$1" --arg b "$2" '[.applications[] | select(.windowId == $a or .windowId == $b)] as $two
    | (first($two[] | select(.windowId == $a)) | .paneStack == [$b])
      or (($two | length) == 2 and ($two | map(.stackId) | unique | length) == 1 and $two[0].stackId != null)'; }
apart() { context --arg a "$1" --arg b "$2" '[.applications[] | select(.windowId == $a or .windowId == $b) | .stackId] as $ids
    | ($ids | length) == 2 and ($ids[0] == null or $ids[1] == null or $ids[0] != $ids[1])'; }
# A card shown on a display fills it; a loose window keeps the size it had.
fills() { probe windowFacts | jq -e --arg id "$1" 'first(.[] | select(.id == $id)) | .width > 1200'; }
x_of() { probe windowFacts | jq -r --arg id "$1" 'first(.[] | select(.id == $id)) | .x'; }
meta() { probe key "$1" 125 >/dev/null; sleep 1.2; }
meta_shift() { probe keyHeld "$1" "${2:-1}" 125 42 >/dev/null; sleep 1.4; }
# Carries the focused window left until it is on the small display.
carry_left() { local _; for _ in 1 2 3; do on_output "$1" Virtual-0 && return 0; meta_shift 105; done; on_output "$1" Virtual-0; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
kscreen-doctor output.Virtual-0.scale.2 >/dev/null 2>&1
sleep 1
kwriteconfig6 --file kaduncerc --group Spread --key Display Virtual-1
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for title in 'Zen' 'Claude'; do client titledCompanion "$title" 500 400; sleep .5; done
for title in 'Zen' 'Claude'; do probe sendCaptionToOutput "$title" Virtual-1; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
zen=$(idOf 'Zen') claude=$(idOf 'Claude')
probe activateWindowId "$zen" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
probe activateWindowId "$zen" >/dev/null
sleep .6
spread_key
pick "$claude"
report start
check 'start: the large display shows a layout of two panes' test "$(pane_count)" -eq 2
check 'start: the small display holds nothing' bash -c "! qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -e '.applications[] | select(.output == \"Virtual-0\" and (.minimized | not))'"

# A pane carried across is the card the small display shows, and comes back.
mapfile -t panes < <(large_panes)
moved=${panes[0]:-$claude}
probe activateWindowId "$moved" >/dev/null
sleep .6
carry_left "$moved"
report carried
check 'carry: Meta+Shift+Left takes the pane to the small display' on_output "$moved" Virtual-0
check 'carry: it shows there' shown_on "$moved" Virtual-0
check 'carry: there it is the card that display shows, not a loose window' is_pane "$moved"
check 'carry: and it fills that display' fills "$moved"
meta_shift 106
report back
check 'back: Meta+Shift+Right brings it to the large display' on_output "$moved" Virtual-1
check 'back: it is a card or a pane there, not a loose window' is_kept "$moved"
check 'back: it takes back the pane it left' is_pane "$moved"
check 'back: beside the pane it left beside' test "$(pane_count)" -eq 2

# Both laid out again, one stacked on the other, and the Stack carried across.
(( $(pane_count) == 2 )) || meta 48
report relaid
check 'stack: Meta+B lays both out again' test "$(pane_count)" -eq 2
mapfile -t panes < <(large_panes)
front=${panes[0]:-$zen} rest=${panes[1]:-$claude}
probe activateWindowId "$rest" >/dev/null
sleep .6
probe activateWindowId "$front" >/dev/null
sleep .6
meta_shift 103
report stacked
check 'stack: Meta+Shift+Up stacks one pane on the other' together "$front" "$rest"
was=$(focused_id)
meta 15
report active-stepped
check 'stack: Meta+Tab steps the Stack while it is the Active card' test "$(focused_id)" != "$was"
check 'stack: and it stays one Stack' together "$front" "$rest"
meta 15
carry_left "$front"
report stack-carried
check 'stack: the Stack reaches the small display' on_output "$front" Virtual-0
check 'stack: there it is the card that display shows' is_pane "$front"
check 'stack: and it fills that display' fills "$front"
check 'stack: its Stack came with it' together "$front" "$rest"
meta 15
report stack-stepped
check 'stack: Meta+Tab there shows the card behind' shown_on "$rest" Virtual-0
check 'stack: and gives it the focus' focused "$rest"
meta_shift 106
report stack-back
check 'stack: Meta+Shift+Right brings the Stack back' on_output "$rest" Virtual-1
check 'stack: it is a card or a pane there' is_kept "$rest"
check 'stack: it takes back the pane it left' is_pane "$rest"
check 'stack: and the Stack stayed whole' together "$rest" "$front"
meta 15
report stack-stepped-back
check 'stack: Meta+Tab there steps its pane' focused "$front"
check 'stack: still one Stack' together "$front" "$rest"

# Laid out again as two panes, Spread shows two cards, not one Stack.
(( $(pane_count) == 2 )) || meta 48
report relaid-again
if (( $(pane_count) < 2 )); then
    meta_shift 108
    report split
fi
check 'spread: two panes on the large display' test "$(pane_count)" -eq 2
spread_key
report spread
check 'spread: Meta+S opens Spread' presents cardLine
check 'spread: the two panes are two cards there, not one Stack' apart "$front" "$rest"
probe key 1 0 >/dev/null
sleep 1.2

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 2 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: desk keys: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: on a desk, panes and Stacks go to the small display and back as cards, and two panes stay two in Spread'
