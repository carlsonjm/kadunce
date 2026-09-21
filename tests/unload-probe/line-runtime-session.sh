#!/usr/bin/env bash
set -euo pipefail
trap 'echo "FAIL: line runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
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
    held=$(kad nativeCarryState | jq -c '.lineRect')
    if [[ $kind == pointer ]]; then probe button false; else probe up 46; fi
    state=$(kad nativeCarryState)
    jq -e --argjson home "$home" --argjson held "$held" \
        '(.lineCarrying|not) and .lineAnimating and .lineRect.y > $home.y
         and .lineRect.y <= $held.y and .lineRect.width >= $held.width
         and .lineRect.width < $home.width' <<<"$state"
    sleep .35
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
# Form a two-card stack using real hold/dwell input.
probe down 51 500 350
sleep .4
probe motion 51 550 350
sleep .45
jq -e '.lineCarrying and .stackArmed' <<<"$(kad nativeCarryState)"
probe up 51
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)
    and .lineRotation >= 0 and .lineRotation < 0.6'
sleep .32
kad nativeCarryState | jq -e '(.lineAnimating|not) and .lineRotation == 0.6'
before=$(kad workspaceContext | jq -c '.cardStage.selectedStack')
jq -e 'length == 2' <<<"$before"
# Short vertical gestures browse, rather than detach, through the same pose
# transition. Interrupt touch browsing with mouse browsing before it settles.
selected_before=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
probe down 54 500 350
probe motion 54 500 410
probe up 54
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" != "$selected_before"
probe pointer 500 350
probe button true
probe pointer 500 290
probe button false
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
sleep .32
kad nativeCarryState | jq -e '(.lineAnimating|not) and (.lineCarrying|not)'
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$selected_before"
test "$(kad workspaceContext | jq -c '.cardStage.selectedStack')" = "$before"
echo 'PASS: touch/mouse stack browsing shares interruptible pose transition without detachment or reordering'
# A detached card dragged away from its armed target must return to its original
# stack/order, not remain detached or commit the old target.
probe down 52 500 350
sleep .4
probe motion 52 550 350
sleep .45
jq -e '.stackArmed' <<<"$(kad nativeCarryState)"
probe motion 52 550 710
jq -e '(.stackArmed|not)' <<<"$(kad nativeCarryState)"
probe up 52
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)
    and .lineRotation >= 0 and .lineRotation < 0.6'
sleep .32
test "$(kad workspaceContext | jq -c '.cardStage.selectedStack')" = "$before"
echo 'PASS: leaving stack target disarms it; no-target release restores original stack order'
echo 'PASS: stack join and return interpolate the held face rotation without retaining input'
# Both seams of the same stack stay reachable without releasing the held card,
# and the seam the page reached is the one the release commits. Insertion depth
# is front-first: depth 0 puts the carried card in front, and a deeper slot
# leaves the destination's face in front. Each release therefore changes which
# card is the face, so the carried identity is re-read before each gesture
# rather than captured once for both.
# Paging runs on a dwell timer, so each assertion waits for it. Reading
# immediately after the motion only reports the depth arming chose.
carried=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
probe down 53 500 350
sleep .4
probe motion 53 550 350
sleep .4
jq -e '.stackArmed and .stackInsertion == 0' <<<"$(kad nativeCarryState)"
probe motion 53 350 350
sleep .34
jq -e '.stackArmed and .stackInsertion == 1' <<<"$(kad nativeCarryState)"
probe up 53
sleep .1
kad workspaceContext | jq -e --arg carried "$carried" \
    '(.cardStage.selectedStack|length) == 2
     and (.cardStage.selectedStack|index($carried)) != null
     and .cardStage.selectedCardId != $carried'
carried=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
probe pointer 500 350
probe button true
sleep .4
probe pointer 450 350
sleep .4
jq -e '.stackArmed and .stackInsertion == 1' <<<"$(kad nativeCarryState)"
probe pointer 720 350
sleep .34
jq -e '.stackArmed and .stackInsertion == 0' <<<"$(kad nativeCarryState)"
probe button false
sleep .1
kad workspaceContext | jq -e --arg carried "$carried" \
    '(.cardStage.selectedStack|length) == 2
     and .cardStage.selectedCardId == $carried'
echo 'PASS: touch and pointer reach both stack seams; depth 0 takes the face and a deeper slot leaves it'
kad nativeCarryState | jq -e '.lineAnimating and (.lineCarrying|not)'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad nativeCarryState | jq -e '(.lineAnimating|not) and (.lineCarrying|not)'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: unload during stack settling leaves no live animation or held card'
