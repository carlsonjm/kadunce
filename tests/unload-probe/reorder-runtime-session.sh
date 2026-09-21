#!/usr/bin/env bash
# CARD-LIFECYCLE.md §6: a carried card moves one position per push distance of
# sideways travel, and the row pages under it while the finger is still down,
# so the release commits an order the user has already been shown. This drives
# the push with real input and requires the row to answer it, to keep answering
# it while the card is held still, to withdraw it, and to leave a push that
# never reached the distance alone.
#
# §9 keeps its precedence: a dwell that never moved the row still aims at a
# stack. This requires the other half of that rule -- that a push far enough to
# move the row does not arm one -- so the two cannot claim the same release.
set -euo pipefail
trap 'echo "FAIL: reorder runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
order() { kad workspaceContext | jq -r '[.applications[] | select(.hasCard)]
    | sort_by(.rowPosition) | map(.windowId | tostring) | join(",")'; }
selected() { kad workspaceContext | jq -r '[.applications[]
    | select(.hasCard and .selected)][0].windowId'; }
reorder_steps() { kad nativeCarryState | jq -r '.lineReorder'; }
stack_armed() { kad nativeCarryState | jq -r '.stackArmed'; }
three_cards() { kad workspaceContext | jq -e '[.applications[]
    | select(.hasCard)] | length == 3'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
client companion
sleep .5
client companion
sleep .7
probe pointer 640 400
kad showCardLine
sleep .4
three_cards
# A 1280x800 work area gives a 762px row pitch, so one position costs 190px of
# travel. 200px asks for exactly one; 120px asks for none.
before="$(order)"
carried="$(selected)"
# The row answers the push before the finger lifts, keeps answering it while
# the card is held there, and arms no stack while it is holding a move. The
# release then commits that move and nothing else.
probe down 60 640 400
sleep .4
probe motion 60 840 400
sleep .35
test "$(reorder_steps)" = 1
echo 'PASS: one push distance moves the row while the card is still held'
sleep .5
test "$(stack_armed)" = false
test "$(reorder_steps)" = 1
echo 'PASS: a push that moved the row aims at no stack, however long it rests'
probe up 60
sleep .6
after="$(order)"
test "$before" != "$after"
test "$(selected)" = "$carried"
three_cards
echo 'PASS: the release commits the order the row was showing'
# Withdrawn before the finger lifts: the row goes back and the release commits
# nothing. Held briefly, because §9 gives a dwell over a card to the stack.
settled="$(order)"
probe down 61 640 400
sleep .4
probe motion 61 840 400
sleep .3
test "$(reorder_steps)" = 1
probe motion 61 640 400
test "$(reorder_steps)" = 0
probe up 61
sleep .6
test "$(order)" = "$settled"
three_cards
echo 'PASS: a withdrawn push leaves the order as it was'
# A push that never reached one position is not a move. This is the travel a
# browse carries, and well inside the old threshold's own slop.
probe down 62 640 400
sleep .4
probe motion 62 760 400
sleep .3
test "$(reorder_steps)" = 0
probe up 62
sleep .6
test "$(order)" = "$settled"
echo 'PASS: a push short of one position leaves the order alone'
test "$(probe releaseRuntime)" = true
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: release returns every window after the reorder gestures'
