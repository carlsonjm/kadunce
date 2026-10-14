#!/usr/bin/env bash
# INPUT.md § Connected, by the keys alone, on two monitors with no touchscreen:
# Virtual-0 at scale 2 is small, sized Solo and left of Virtual-1, which is
# large, sized Connected and chosen for Spread. Meta's arrows move the focus
# from pane to pane and on to the small display; with Shift a pane trades
# places with the one beside it and, at the layout's edge, goes on to the
# small display and back. The brackets move a divider. Meta+Shift+Up puts a
# pane with no Stack into the Stack of the pane used before it, and puts a
# Stack's card deeper, placed when Meta is let go; Meta+Tab steps the Stack
# and Meta+Shift+Down takes a card out of it, or a pane out of the layout, and
# in Spread the centred card out of its Stack. A pane carried to the small
# display takes its Stack with it.
# Every check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected keys: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, output, hasCard, pane, paneStack, focused, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden})')"
}
spread_key() { probe key 31 125 >/dev/null; sleep 1; }
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
app() { kad workspaceContext | jq -c --arg id "$1" 'first(.applications[] | select(.windowId == $id))'; }
focused_id() { kad workspaceContext | jq -r 'first(.applications[] | select(.focused)) | .windowId'; }
focused() { [[ $(focused_id) == "$1" ]]; }
output_of() { kad workspaceContext | jq -r --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .output'; }
on_output() { [[ $(output_of "$1") == "$2" ]]; }
is_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .pane and (.hasCard | not) and (.minimized | not)'; }
is_card() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard'; }
large_panes() { kad workspaceContext | jq -r '[.applications[] | select(.pane and (.hasCard | not) and .output == "Virtual-1") | .windowId] | .[]'; }
behind_is() { context --arg id "$1" --argjson ids "$2" 'first(.applications[] | select(.windowId == $id)) | .paneStack == $ids'; }
x_of() { probe windowFacts | jq -r --arg id "$1" 'first(.[] | select(.id == $id)) | .x'; }
width_of() { probe windowFacts | jq -r --arg id "$1" 'first(.[] | select(.id == $id)) | .width'; }
y_of() { probe windowFacts | jq -r --arg id "$1" 'first(.[] | select(.id == $id)) | .y'; }
# The large display's pane furthest left, the upper one of a column.
leftmost() {
    local id best="" bx=999999 by=999999 x y
    for id in $(large_panes); do
        x=$(x_of "$id") y=$(y_of "$id")
        if (( x < bx || (x == bx && y < by) )); then bx=$x by=$y best=$id; fi
    done
    echo "$best"
}
left_edge() { local id min=999999 x; for id in $(large_panes); do x=$(x_of "$id"); (( x < min )) && min=$x; done; echo "$min"; }
shown_on() { probe windowFacts | jq -e --arg id "$1" --arg o "$2" 'first(.[] | select(.id == $id)) | .output == $o and (.hidden | not)'; }
focus_on() { probe activateWindowId "$1" >/dev/null; sleep .6; }
meta() { probe key "$1" 125 >/dev/null; sleep 1.2; }
meta_shift() { probe keyHeld "$1" "${2:-1}" 125 42 >/dev/null; sleep 1.4; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
kscreen-doctor output.Virtual-0.scale.2 >/dev/null 2>&1
sleep 1
echo "displays: $(kscreen-doctor -o 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | grep -E 'Output|Geometry|Scale' | tr -s ' \n' ' ')"
kwriteconfig6 --file kaduncerc --group Spread --key Display Virtual-1
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for title in 'Small' 'One' 'Two' 'Three' 'Four' 'Five'; do client titledCompanion "$title" 500 400; sleep .5; done
probe sendCaptionToOutput 'Small' Virtual-0
for title in 'One' 'Two' 'Three' 'Four' 'Five'; do probe sendCaptionToOutput "$title" Virtual-1; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
small=$(idOf 'Small') one=$(idOf 'One') two=$(idOf 'Two') three=$(idOf 'Three') four=$(idOf 'Four') five=$(idOf 'Five')
probe activateWindowId "$one" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
# The large display's own card is the one in use when its layout is made.
probe activateWindowId "$one" >/dev/null
sleep .6
# Three panes on the large display, picked from Spread into its layout.
spread_key
pick "$two"
spread_key
pick "$three"
report start
check 'start: the large display shows three panes' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce workspaceContext | jq '[.applications[] | select(.pane and (.hasCard | not) and .output == \"Virtual-1\")] | length') == 3 ]]"
check 'start: the small display shows its card' is_pane "$small"

# Focus: along the layout, then off its left edge to the small display.
left=$(leftmost)
focus_on "$left"
meta 106
report focus-right
beside=$(focused_id)
check 'focus: Meta+Right reaches another pane' bash -c "[[ $beside != $left && \$(qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -r --arg id $beside 'first(.applications[] | select(.windowId == \$id)) | .output') == Virtual-1 ]]"
meta 105
report focus-left
check 'focus: Meta+Left comes back to a pane left of it' bash -c "(( \$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -r --arg id \$(qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -r 'first(.applications[] | select(.focused)) | .windowId') 'first(.[] | select(.id == \$id)) | .x') < $(x_of "$beside") ))"
focus_on "$left"
meta 105
report focus-small
check 'focus: past the layout edge Meta+Left reaches the small display' focused "$small"
meta 106
report focus-back
check 'focus: Meta+Right from the small display reaches the layout at its left edge' bash -c "(( \$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -r --arg id \$(qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -r 'first(.applications[] | select(.focused)) | .windowId') 'first(.[] | select(.id == \$id)) | .x') == $(left_edge) ))"
check 'focus: no pane moved' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce workspaceContext | jq '[.applications[] | select(.pane and (.hasCard | not) and .output == \"Virtual-1\")] | length') == 3 ]]"

# Carry: the pane beside trades places, then goes on to the small display.
focus_on "$beside"
before_beside=$(x_of "$beside")
meta_shift 105
report swapped
check 'carry: Meta+Shift+Left trades the pane with the one left of it' bash -c "(( \$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -r --arg id $beside 'first(.[] | select(.id == \$id)) | .x') < $before_beside ))"
check 'carry: it is now at the layout edge' bash -c "(( \$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -r --arg id $beside 'first(.[] | select(.id == \$id)) | .x') == \$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -r 'map(select(.output == \"Virtual-1\" and (.hidden | not) and .width < 2500)) | map(.x) | min') ))"
check 'carry: it keeps the focus' focused "$beside"
meta_shift 105
report sent-small
check 'carry: again at the edge, it goes to the small display' on_output "$beside" Virtual-0
check 'carry: the small display shows it' shown_on "$beside" Virtual-0
check 'carry: there it is a card of that display' context --arg id "$beside" 'first(.applications[] | select(.windowId == $id)) | .pane or .hasCard'
focus_on "$beside"
meta_shift 106
report sent-back
check 'carry: Meta+Shift+Right brings it back to the large display' on_output "$beside" Virtual-1

# Divider: the brackets move the edge beside the focused pane.
left=$(leftmost)
focus_on "$left"
width=$(width_of "$left")
meta 27
report divider-right
check 'divider: Meta+] widens the left pane' bash -c "(( \$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -r --arg id $left 'first(.[] | select(.id == \$id)) | .width') >= $width + 60 ))"
wider=$(width_of "$left")
meta 26
report divider-left
check 'divider: Meta+[ narrows it again' bash -c "(( \$(qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -r --arg id $left 'first(.[] | select(.id == \$id)) | .width') <= $wider - 60 ))"

# Stacks: a pane with none joins the Stack of the pane used before it.
# Five panes, so two joins and a pane let go still leave a layout for a
# pane holding a Stack to be carried out of.
spread_key
pick "$four"
spread_key
pick "$five"
mapfile -t panes < <(large_panes)
check 'stacks: the layout holds five panes' test "${#panes[@]}" -eq 5
first=${panes[0]:-} second=${panes[1]:-} third=${panes[2]:-} fourth=${panes[3]:-}
focus_on "$second"
focus_on "$first"
meta_shift 103
report joined
check 'join: Meta+Shift+Up puts the pane in front of the one used before it' behind_is "$first" "[\"$second\"]"
check 'join: that one waits behind it' is_card "$second"
check 'join: the other pane stayed' is_pane "$third"
meta 15
report tabbed
check 'step: Meta+Tab shows the next card of the Stack' is_pane "$second"
check 'step: the first waits behind it' behind_is "$second" "[\"$first\"]"
probe keyHeld 15 1 125 42 >/dev/null
sleep 1.4
report tabbed-back
check 'step: Meta+Shift+Tab steps back' is_pane "$first"

# A third card, then the ladder: one press puts the card one deeper, and
# Meta let go places it there.
focus_on "$first"
focus_on "$third"
meta_shift 103
report joined-three
check 'join: the third joins in front' behind_is "$third" "[\"$first\",\"$second\"]"
meta_shift 103 1
report deeper-one
check 'deeper: Meta+Shift+Up once puts it behind the next card' behind_is "$first" "[\"$third\",\"$second\"]"
check 'deeper: the card it passed is in front' is_pane "$first"
focus_on "$first"
meta_shift 103 2
report deeper-two
check 'deeper: held, two presses put it at the back' behind_is "$third" "[\"$second\",\"$first\"]"
focus_on "$third"
meta_shift 103 3
report deeper-round
check 'deeper: past the back it comes round to where it was' behind_is "$third" "[\"$second\",\"$first\"]"

# Down lets go: out of the Stack, then out of the layout.
focus_on "$third"
meta_shift 108
report let-go
check 'let go: Meta+Shift+Down takes the card out of its Stack' behind_is "$third" '[]'
check 'let go: it joins the layout as a pane of its own' is_pane "$third"
check 'let go: the next card takes the pane' behind_is "$second" "[\"$first\"]"
check 'let go: it is a pane' is_pane "$second"
focus_on "$fourth"
meta_shift 108
report left-layout
check 'let go: a pane with no Stack leaves the layout as a card' is_card "$fourth"

# A pane carried to another display takes its whole Stack with it.
together() { context --arg a "$1" --arg b "$2" '[.applications[] | select(.windowId == $a or .windowId == $b)] as $two
    | (first($two[] | select(.windowId == $a)) | .paneStack == [$b])
      or (($two | length) == 2 and ($two | map(.stackId) | unique | length) == 1 and $two[0].stackId != null)'; }
focus_on "$second"
for _ in 1 2 3 4 5; do
    on_output "$second" Virtual-0 && break
    meta_shift 105
    focus_on "$second"
done
report stack-carried
check 'carry: a pane holding a Stack reaches the small display' on_output "$second" Virtual-0
check 'carry: its Stack came with it, in order' together "$second" "$first"
check 'carry: there it is the card that display shows' is_pane "$second"
meta 15
report stack-carried-step
check 'carry: Meta+Tab steps that Stack on the small display' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce workspaceContext | jq -r --arg id $first 'first(.applications[] | select(.windowId == \$id)) | .output') == Virtual-0 ]]"
check 'carry: the card behind now shows there' focused "$first"

# In Spread, Meta+Shift+Down takes the centred card out of its Stack, as a
# pull down does; the Stack keeps the pane with its next card.
# The card let go earlier comes back, so the layout outlasts the join.
fifth=${panes[4]:-}
spread_key
pick "$fourth"
check 'spread: the card let go earlier is a pane again' is_pane "$fourth"
focus_on "$fifth"
focus_on "$third"
meta_shift 103
report spread-joined
check 'spread: a Stack to let go of in Spread' behind_is "$third" "[\"$fifth\"]"
spread_key
select_card "$third" || echo "could not select $third"
meta_shift 108
report spread-let-go
check 'spread: Meta+Shift+Down takes the centred card out of its Stack' \
    context --arg id "$third" 'first(.applications[] | select(.windowId == $id)) | .stackSize == 1'
check 'spread: Spread stays open' context '.cardStage.presentation == "cardLine"'
select_card "$fifth" || echo "could not select $fifth"
open_selected
report spread-let-go-open
check 'spread: the Stack keeps the pane' is_pane "$fifth"
check 'spread: the card let go is no pane' is_card "$third"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 6 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: connected keys: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: the keys move the focus and the panes, across displays, and order and let go of Stacks'
