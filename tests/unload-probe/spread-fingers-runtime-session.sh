#!/usr/bin/env bash
# Three fingers down open Spread from the Active card in the middle of five,
# and the row must stay centred on that card: through KWin's own recogniser
# (this scene runs with global shortcuts on, which the recogniser needs), as
# Meta+S does, and whatever else the tablet sends while the fingers are down.
# On the tablet a scroll arrives with the fingers; before Spread forms it is the
# application's, and each one that lands on the forming row moved it a card.
# SPREAD_KIT=1 adds the Z13 kit's posture file (Kadunce's direct edges).
set -euo pipefail
trap 'echo "FAIL: spread fingers line $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
if [[ ${SPREAD_KIT:-0} == 1 ]]; then
    mkdir -p "$XDG_RUNTIME_DIR/z13-tablet-kit"
    echo tablet >"$XDG_RUNTIME_DIR/z13-tablet-kit/posture"
fi
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe pointer 300 200
for card in "B 2e8b57" "C 8b2e57" "D 2e578b" "E 8b8b2e"; do
    set -- $card
    client colouredCompanion "Card $1" "$2" 600 450
    sleep .3
done
sleep .3
probe entryClientsOnTablet
sleep .3
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .5
kad workspaceContext | jq -e '[.displayContext.displays[] | select(.name == "Virtual-0" and .role == "tablet")] | length == 1'
kad workspaceContext | jq -c '.displayContext | {edgeBackend, posture}'
# The Z13's panel, so KWin's recogniser measures a swipe as on the device.
test "$(probe givePhysicalSize Virtual-0 288 180)" = true
read -r W H < <(kad workspaceContext | jq -r '.displayContext.displays[] | select(.name == "Virtual-0") | "\(.geometry.width) \(.geometry.height)"')
echo "TABLET ${W}x${H}"

# One line per sample: presentation, the selected card's entry, and each card's
# drawn x by entry (null when not drawn).
sample() {
    kad workspaceContext | jq -c '{p: .cardStage.presentation,
        sel: ([.applications[] | select(.selected) | .entry] | first),
        x: ([.applications[] | select(.hasCard)] | sort_by(.entry) | map(.spreadRect.x))}'
}
selected() { kad workspaceContext | jq '[.applications[] | select(.selected) | .entry] | first'; }
watch_for() { # label count
    local label=$1 n=$2
    for ((i = 0; i < n; i++)); do echo "$label $(sample)"; sleep .1; done
}
to_middle() {
    kad showCardLine
    sleep .5
    for attempt in {1..6}; do
        s=$(selected)
        [[ $s == 2 ]] && break
        if ((s > 2)); then probe key 105 0; else probe key 106 0; fi
        sleep .5
    done
    test "$(selected)" = 2
    kad showActive
    sleep .6
}
# A real three-finger swipe down through KWin's recogniser: three contacts
# within 250 ms and a few millimetres of each other, moved down together, with
# $3 run after each frame of motion (what else the tablet sends meanwhile), and
# $5 between one lift and the next.
real_swipe() { # label base-id alongside start-fraction between-lifts
    local label=$1 id=$2 alongside=${3:-} frac=${4:-0.3} between=${5:-}
    local x0=$((W / 2 - 60)) y0 y step
    y0=$(awk -v h="$H" -v f="$frac" 'BEGIN { printf "%d", h * f }')
    probe down "$id" "$x0" "$y0"
    probe down $((id + 1)) $((x0 + 60)) "$y0"
    probe down $((id + 2)) $((x0 + 120)) "$y0"
    for step in $(seq 1 20); do
        y=$((y0 + step * 12))
        probe motion "$id" "$x0" "$y"
        probe motion $((id + 1)) $((x0 + 60)) "$y"
        probe motion $((id + 2)) $((x0 + 120)) "$y"
        [[ -n $alongside ]] && eval "$alongside"
        ((step % 5 == 0)) && echo "$label-move $step $(sample)"
    done
    probe up "$id"
    [[ -n $between ]] && eval "$between"
    probe up $((id + 1))
    [[ -n $between ]] && eval "$between"
    probe up $((id + 2))
    watch_for "$label" 8
}

to_middle
echo "ACTIVE $(sample)"
fail=0
expect() { # label expected-entry
    local got
    got=$(selected)
    echo "RESULT $1 selected=$got expected=$2"
    [[ $got == "$2" ]] || fail=1
}

echo '--- Meta+S'
kad showCardLine
watch_for A 5
expect meta-s 2
to_middle

echo '--- three fingers alone'
real_swipe C 60
expect fingers 2
to_middle

echo '--- three fingers with a continuous scroll alongside'
real_swipe S 70 'probe fingerScroll 6'
expect fingers-with-scroll 2
to_middle

echo '--- three fingers with wheel notches alongside'
real_swipe N 80 'probe wheel 1'
expect fingers-with-wheel 2
to_middle

echo '--- three fingers lifted one by one, with a scroll between the lifts'
real_swipe L 100 '' 0.3 'probe fingerScroll 6; probe fingerScroll 6; probe fingerScroll 6'
expect fingers-scroll-between-lifts 2
to_middle

echo '--- a scroll once Spread is open still moves the row, as INPUT.md says'
kad showCardLine
sleep .4
probe wheel 1
sleep .3
expect open-row-wheel 1
to_middle

# A separate route to the same symptom, not the tablet's (its log shows no
# promotion before an opening): a first contact in the gutter above the Active
# card reaches a hidden card's own window, KWin activates it on touch-down, and
# that card becomes Active before Spread forms. Reported, not asserted.
echo '--- first contact in the gutter above the Active card, over a hidden card'
real_swipe G 90 '' 0.01
echo "NOTE fingers-from-gutter selected=$(selected) (2 would be where the person was)"

# INPUT.md: a card chosen in Spread grows into the Active card rather than
# cutting to it. Enter takes the same path as a tap.
echo '--- Enter in Spread grows the card into Active'
kad showCardLine
sleep .5
rest_w=$(kad workspaceContext | jq '[.applications[] | select(.selected) | .spreadRect.width] | first')
probe key 28 0
sleep .08
mid=$(kad workspaceContext | jq -c '{p: .cardStage.presentation, w: ([.applications[] | select(.selected) | .spreadRect.width] | first)}')
sleep .6
after=$(kad workspaceContext | jq -r '.cardStage.presentation')
echo "RESULT enter-grows rest=$rest_w mid=$mid after=$after"
jq -e --argjson r "$rest_w" '.p == "cardLine" and .w > $r' <<<"$mid" >/dev/null || fail=1
[[ $after == active ]] || fail=1
test "$(selected)" = 2 || fail=1

echo "SUMMARY fail=$fail"
((fail == 0))
echo 'PASS: three fingers open Spread on the card the person was on, whatever arrives with them'
