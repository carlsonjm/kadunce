#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: line runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
source "$(dirname "${BASH_SOURCE[0]}")/spread-carry.bash"
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
# A plain-card no-target drop settles visually while membership and native
# placement remain unchanged. Both contact types must drain at release.
# The settle is read one D-Bus round trip after release, so it has already
# advanced by then: every term is a bound between held and home, never an
# equality with either. An exact held width could only hold on a read that
# elapsed no time, which this one cannot.
for kind in pointer touch; do
    probe pointer 500 350
    kad showCardLine
    sleep .35
    home=$(kad nativeCarryState | jq -c '.lineRect')
    if [[ $kind == pointer ]]; then probe button true; else probe down 46 500 350; fi
    sleep .4
    if [[ $kind == pointer ]]; then probe pointer 500 570; else probe motion 46 500 570; fi
    # Picked up, the card rises; carried, it stays a little larger than the
    # row, and a lone card has no row to zoom out, however far it is pulled.
    sleep .3
    held=$(kad nativeCarryState | jq -c '.lineRect')
    jq -e --argjson home "$home" '.width > $home.width' <<<"$held"
    if [[ $kind == pointer ]]; then probe button false; else probe up 46; fi
    state=$(kad nativeCarryState)
    jq -e --argjson home "$home" --argjson held "$held" \
        '(.lineCarrying|not) and .lineAnimating and .lineRect.y > $home.y
         and .lineRect.y <= $held.y and .lineRect.width <= $held.width
         and .lineRect.width > $home.width' <<<"$state"
    sleep .55
    kad nativeCarryState | jq -e --argjson home "$home" \
        '(.lineAnimating|not) and .lineRect == $home'
done
echo 'PASS: mouse/touch plain-card release settles from held position to unchanged home'
# Unloading during the visual settle must not wait for it or retain callbacks.
probe pointer 500 350
probe button true
sleep .4
probe pointer 500 570
probe button false
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .35
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad nativeCarryState | jq -e '(.lineAnimating|not) and (.lineCarrying|not) and (.inputBusy|not)'
echo 'PASS: unload during settle leaves a fresh effect with no animation or held input'
for scenario in open edge withdrawn; do
for kind in pointer touch; do
    probe pointer 500 350
    kad showCardLine
    sleep .3
    if [[ $kind == pointer ]]; then probe button true; else probe down 47 500 350; fi
    sleep .4
    jq -e '.lineCarrying' <<<"$(kad nativeCarryState)"
    x=1800; if [[ $scenario != open ]]; then x=2555; fi
    if [[ $kind == pointer ]]; then probe pointer "$x" 380; else probe motion 47 "$x" 380; fi
    jq -e '.lineCarrying and .lineDestination and .destinationPreview' <<<"$(kad nativeCarryState)"
    if [[ $scenario == withdrawn ]]; then
        if [[ $kind == pointer ]]; then probe pointer 1800 380; else probe motion 47 1800 380; fi
    fi
    destination=$(kad nativeCarryState | jq -c '.destinationRect')
    window_id=$(kad workspaceContext | jq -r '.applications[0].windowId')
    if [[ $kind == pointer ]]; then probe button false; else probe up 47; fi
    if [[ $scenario == edge ]]; then
        kad nativeCarryState | jq -e '.dropSettling and (.lineCarrying|not)'
        if [[ $kind == pointer ]]; then
            probe pointer 1800 400; probe button true
        else probe down 48 1800 400; fi
        kad nativeCarryState | jq -e '(.dropSettling|not)'
        if [[ $kind == pointer ]]; then probe button false; else probe up 48; fi
    fi
    sleep .15
    jq -e '(.lineCarrying|not) and (.lineDestination|not) and (.destinationPreview|not)' <<<"$(kad nativeCarryState)"
    probe windowGeometry "$window_id" | jq -e --argjson target "$destination" '. == $target'
    kad workspaceContext | jq -e '[.applications[] | select(.output == "Virtual-1")] | length == 1'
    if [[ $scenario == edge ]]; then
        kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-1" and .bentoActive)] | length == 1'
    else
        kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-1" and .bentoActive)] | length == 0'
        test "$(kad toggleBentoOnOutput Virtual-1)" = true
    fi
    test "$(kad handoffBentoLeadToOutput Virtual-1 Virtual-0)" = true
    sleep .2
done
done
echo 'PASS: Spread mouse/touch open, edge and withdrawn-edge use reserved receivers'
echo 'PASS: monitor edge settling releases drag ownership and fresh pointer/touch retires animation'
client companion
sleep .6
probe pointer 500 350
kad showCardLine
sleep .3
# Form a two-card stack as a person does: carry one card onto the other and
# rest there.
carry_onto_centre 51
probe up 51
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)
    and .lineRotation >= 0 and .lineRotation < 0.6'
sleep .55
kad nativeCarryState | jq -e '(.lineAnimating|not) and .lineRotation == 0.6'
before=$(kad workspaceContext | jq -c '.cardStage.selectedStack')
jq -e 'length == 2' <<<"$before"
# A short vertical stroke on a Stack neither browses it nor takes a card out:
# up closes and down takes a card out only past their own distances. A slow
# sideways stroke begun on the centred Stack thumbs through it instead, by
# touch and by mouse, through the same interruptible pose transition.
selected_before=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
probe down 54 500 350
sleep .1
probe motion 54 500 410
sleep .1
probe up 54
sleep .4
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$selected_before"
test "$(kad workspaceContext | jq -c '.cardStage.selectedStack')" = "$before"
probe down 54 500 350
sleep .1
probe motion 54 490 350
sleep .1
probe motion 54 400 350
probe up 54
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" != "$selected_before"
probe pointer 500 350
probe button true
sleep .1
probe pointer 510 350
sleep .1
probe pointer 600 350
probe button false
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
sleep .32
kad nativeCarryState | jq -e '(.lineAnimating|not) and (.lineCarrying|not)'
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$selected_before"
test "$(kad workspaceContext | jq -c '.cardStage.selectedStack')" = "$before"
echo 'PASS: a short vertical stroke leaves a Stack alone; a slow sideways stroke thumbs through it by touch and mouse'
# A Stack's card held is lifted out of its Stack and the row stands: each
# slow-stroke step sideways takes it a place through the Stack, counted from
# the front, a finger moving left sending it back, and nothing carries it off
# however far along or down the finger goes. The front card held and let go a
# place back, the card that was behind it is the one in front.
rest=$(kad workspaceContext | jq -c '[.applications[] | select(.hasCard and .selected)][0].spreadRect')
probe down 52 500 350
sleep .5
held=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
pickup=$(kad nativeCarryState | jq -c '.lineRect')
kad nativeCarryState | jq -e '.lineCarrying and .carryIndex == -1'
probe motion 52 470 350
sleep .1
probe motion 52 380 350
sleep .3
kad nativeCarryState | jq -e '.lineCarrying and .carryAim == "place" and .carryIndex == 1'
probe motion 52 140 700
sleep .2
kad nativeCarryState | jq -e --argjson pickup "$pickup" --argjson rest "$rest" \
    '.lineCarrying and .carryAim == "place" and .carryIndex == 1 and (.lineDestination|not)
     and ((.lineRect.x + .lineRect.width / 2) - ($pickup.x + $pickup.width / 2) | abs) < 60
     and ((.lineRect.y + .lineRect.height / 2) - ($pickup.y + $pickup.height / 2) | abs) < 60
     and .lineRect.y < $rest.y'
probe up 52
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
sleep .45
kad workspaceContext | jq -e --argjson before "$before" --arg held "$held" \
    '(.cardStage.selectedStack | sort) == ($before | sort) and .cardStage.selectedCardId != $held'
echo 'PASS: a Stack card held stays over its Stack, pages a place for each step, and let go behind shows the card in front'
echo 'PASS: stack join and return interpolate the held face rotation without retaining input'
# Spread's side edges only move the row under a held card: let go at the
# right edge, the card is an ordinary drop, and nothing pairs into Bento.
probe down 55 500 350
sleep .4
probe motion 55 1275 350
sleep .2
jq -e '.lineCarrying and (.lineDestination|not)' <<<"$(kad nativeCarryState)"
probe up 55
sleep .6
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"
    and ([.applications[] | select(.hasCard and (.minimized|not))] | length) == 2'
echo "PASS: a card let go at Spread's side edge pairs nothing"
# Resting on a card joins it at the front: the carried card is the Stack's
# face. The two cards are separated first if the drop above left them stacked.
kad showActive
sleep .5
kad showCardLine
sleep .5
if kad workspaceContext | jq -e '(.cardStage.selectedStack|length) == 2' >/dev/null; then
    probe down 56 500 350
    sleep .1
    probe motion 56 500 362
    sleep .05
    probe motion 56 500 540
    probe up 56
    sleep .6
fi
kad workspaceContext | jq -e '(.cardStage.selectedStack|length) == 1'
carried=$(kad workspaceContext | jq -r '[.applications[] | select(.hasCard and (.selected|not))][0].windowId')
carry_onto_centre 53
probe up 53
kad workspaceContext | jq -e --arg carried "$carried" \
    '(.cardStage.selectedStack|length) == 2 and .cardStage.selectedCardId == $carried'
echo 'PASS: resting on a card joins it at the front'
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad nativeCarryState | jq -e '(.lineAnimating|not) and (.lineCarrying|not)'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: unload during stack settling leaves no live animation or held card'
# Spread 2.0. A card pulled down out of a Stack stands alone just after it and
# the Stack stays selected; the row springs back from its first end without
# paging, and a flick lands on the next card; a flick up closes the app under
# the finger. The reload starts from two individual cards.
cards() { kad workspaceContext | jq '[.applications[] | select(.hasCard)] | length'; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .3
probe pointer 500 350
kad showCardLine
sleep .5
carry_onto_centre 60
probe up 60
sleep .5
kad workspaceContext | jq -e '(.cardStage.selectedStack|length) == 2'
face=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
probe down 61 500 350
sleep .1
probe motion 61 500 362
sleep .05
probe motion 61 500 540
probe up 61
sleep .5
kad workspaceContext | jq -e --arg face "$face" \
    '(.cardStage.selectedStack|length) == 1 and .cardStage.selectedCardId != $face'
echo 'PASS: a card pulled down out of a Stack stands alone and the Stack stays selected'
first=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
probe down 62 500 350
sleep .1
probe motion 62 512 350
probe motion 62 800 350
sleep .15
probe up 62
sleep 1.2
kad nativeCarryState | jq -e '(.lineAnimating|not)'
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$first"
probe down 63 700 350
sleep .05
probe motion 63 688 350
sleep .02
probe motion 63 400 350
sleep .02
probe motion 63 300 350
probe up 63
sleep 1.8
kad nativeCarryState | jq -e '(.lineAnimating|not)'
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$face"
echo 'PASS: the row springs back from its first end and a flick lands on the next card'
client companion
sleep 1.2
kad showCardLine
sleep .6
before_close=$(cards)
closing=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
test "$closing" != "$face" && test "$closing" != "$first"
probe down 64 500 350
sleep .05
probe motion 64 500 338
sleep .02
probe motion 64 500 150
probe up 64
sleep 1.5
test "$(cards)" -eq $((before_close - 1))
kad workspaceContext | jq -e --arg closing "$closing" \
    '[.applications[] | select(.windowId == $closing)] | length == 0'
# The closed window had the focus, and KWin handing it on is not a choice of
# card: Spread stays.
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"'
echo 'PASS: a flick up closes the app under the finger and Spread stays'
# A tap opens any card it lands on, the one beside the centre included, and a
# tap on empty space goes back to the card the person came from.
context=$(kad workspaceContext)
width=$(jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width' <<<"$context")
read -r side_x side_y side_id < <(jq -r --argjson width "$width" \
    '[.applications[] | select(.hasCard and (.selected|not) and .spreadRect)][0]
     | "\(((([.spreadRect.x, 0] | max) + ([.spreadRect.x + .spreadRect.width, $width] | min)) / 2) | floor) \((.spreadRect.y + .spreadRect.height / 2) | floor) \(.windowId)"' <<<"$context")
probe down 65 "$side_x" "$side_y"
sleep .05
probe up 65
sleep .8
kad workspaceContext | jq -e --arg id "$side_id" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
kad showCardLine
sleep .6
probe down 66 20 60
sleep .05
probe up 66
sleep .8
kad workspaceContext | jq -e --arg id "$side_id" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
echo 'PASS: a tap opens the card beside the centre, and a tap on empty space goes back'
# A long row carried stays three across, leaned or not. Pulled down past a
# short way it zooms out to one set view, the same however far it is pulled,
# and pushed back up it returns to three across.
for n in 1 2 3 4 5; do client titledCompanion "Long row $n" 500 400; done
sleep 1.5
kad showCardLine
sleep .6
test "$(cards)" -ge 7
middle=$(($(kad workspaceContext | jq '.displayContext.displays[] | select(.role == "tablet")
    | .geometry.x + .geometry.width / 2 | floor')))
edge=$(($(kad workspaceContext | jq '.displayContext.displays[] | select(.role == "tablet")
    | .geometry.x + .geometry.width - 3')))
probe down 67 "$middle" 350
sleep .5
probe motion 67 $((middle + 30)) 350
sleep .5
kad nativeCarryState | jq -e '.lineCarrying and .carryScale == 1'
probe motion 67 "$edge" 350
sleep .5
kad nativeCarryState | jq -e '.lineCarrying and .carryScale == 1'
probe motion 67 "$middle" 350
sleep .2
probe motion 67 "$middle" 520
sleep .6
zoomed=$(kad nativeCarryState | jq '.carryScale')
jq -en --argjson zoomed "$zoomed" '$zoomed < 0.5'
probe motion 67 "$middle" 720
sleep .4
kad nativeCarryState | jq -e --argjson zoomed "$zoomed" '.lineCarrying and (.carryScale - $zoomed | abs) < 0.001'
probe motion 67 "$middle" 380
sleep .6
kad nativeCarryState | jq -e '.lineCarrying and .carryScale == 1'
probe up 67
sleep .8
kad nativeCarryState | jq -e '(.lineAnimating|not) and (.lineCarrying|not)'
echo 'PASS: a long row stays three across, leaned or not, and a pull down zooms it out to one set view and back'
# A card held when another app closes is let go: it glides back from where it
# is drawn to its place as the row closes up, rather than jumping there. The
# glide is read the moment the hold ends, so every term is a bound between
# held and home, and it has landed a while after.
home=$(kad nativeCarryState | jq -c '.lineRect')
held_title=$(kad workspaceContext | jq -r '[.applications[] | select(.selected) | .title] | first')
gone="Long row 1"
[[ $held_title != "$gone" ]] || gone="Long row 2"
before_cards=$(cards)
probe down 68 "$middle" 350
sleep .4
probe motion 68 "$middle" 450
sleep .3
held=$(kad nativeCarryState | jq -c '.lineRect')
kad nativeCarryState | jq -e '.lineCarrying'
client closeCompanion "$gone"
for _ in {1..40}; do
    state=$(kad nativeCarryState)
    if jq -e '.lineCarrying | not' <<<"$state" >/dev/null; then break; fi
    sleep .01
done
probe up 68
echo "RESULT cancel-glide home=$home held=$held state=$(jq -c '{lineAnimating, lineRect}' <<<"$state")"
jq -e --argjson home "$home" --argjson held "$held" \
    '(.lineCarrying|not) and .lineAnimating and .lineRect.y > $home.y
     and .lineRect.y <= $held.y and .lineRect.width > $home.width' <<<"$state"
sleep .6
kad nativeCarryState | jq -e --argjson home "$home" '(.lineAnimating|not) and .lineRect == $home'
test "$(cards)" -eq $((before_cards - 1))
test "$(kad workspaceContext | jq -r '[.applications[] | select(.selected) | .title] | first')" = "$held_title"
echo 'PASS: a held card whose grab another app cancels glides back to its place'
# Kadunce's keys sit on Meta, and Ctrl stays with applications (INPUT.md §
# Keyboard shortcuts). Linux key codes: 125 Meta, 29 Ctrl, 31 S, 105 Left,
# 106 Right, 1 Escape; the probe holds the modifier around the key.
chord() { probe key "$2" "$1"; }
selected() { kad workspaceContext | jq -r '.cardStage.selectedCardId'; }
kad showActive
sleep .5
before=$(selected)
chord 29 31
sleep .4
kad workspaceContext | jq -e '.cardStage.presentation == "active"'
chord 125 105
sleep .5
if [[ $(selected) == "$before" ]]; then chord 125 106; sleep .5; fi
test "$(selected)" != "$before"
chord 125 31
sleep .4
kad workspaceContext | jq -e '.cardStage.presentation == "cardLine"'
probe key 1 0
sleep .6
kad workspaceContext | jq -e '.cardStage.presentation == "active"'
echo 'PASS: Meta moves between cards and opens Spread, Escape goes back, and Ctrl+S stays with the application'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
