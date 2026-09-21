#!/usr/bin/env bash
# CARD-LIFECYCLE.md §9: an ordinary stack releases a member when the member is
# lifted and pulled up out of the stack, and rejoins one that never rose out of
# it. Sideways travel still reorders, so this drives both with real input and
# requires each to leave the other's answer alone.
set -euo pipefail
trap 'echo "FAIL: stack runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
cards() { kad workspaceContext | jq -c '[.applications[] | select(.hasCard) | {s: .stackId, p: .stackPosition, n: .stackSize}] | sort_by(.s, .p)'; }
stacked_pair() { kad workspaceContext | jq -e '[.applications[] | select(.hasCard)] as $c
    | ($c | length) == 2 and ([$c[].stackId] | unique | length) == 1 and all($c[]; .stackSize == 2)'; }
separate_cards() { kad workspaceContext | jq -e '[.applications[] | select(.hasCard)] as $c
    | ($c | length) == 2 and ([$c[].stackId] | unique | length) == 2 and all($c[]; .stackSize == 1 and (.minimized | not))'; }
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
sleep .6
probe pointer 500 350
kad showCardLine
sleep .3
separate_cards
# Form a two-card stack with the hold and dwell the gesture requires.
probe down 51 500 350
sleep .4
probe motion 51 550 350
sleep .45
jq -e '.lineCarrying and .stackArmed' <<<"$(kad nativeCarryState)"
probe up 51
sleep .5
stacked_pair
echo 'PASS: two cards form one ordinary stack'
# §9: a release that never rose out of the stack rejoins it. This is the browse
# distance, well inside the rise the release asks for.
probe down 52 500 350
sleep .4
probe motion 52 500 300
sleep .3
probe up 52
sleep .6
stacked_pair || { echo "DIAG rejoin: $(cards)"; false; }
echo 'PASS: a lift that never rose out of the stack rejoins it'
# §9: pulled up out of the stack, the member is released into the Spread where
# the stack stands. Neither window is minimized, lost, or handed to Plasma.
probe down 53 500 350
sleep .4
probe motion 53 500 120
sleep .3
probe up 53
sleep .6
separate_cards || { echo "DIAG release: $(cards)"; false; }
kad workspaceContext | jq -e '.cardStage.active'
echo 'PASS: a member pulled up out of the stack is released into the Spread'
# §9: sideways travel still reorders rather than releasing, so the two answers
# do not compete for one gesture. Rebuild the stack and page it instead.
probe down 54 500 350
sleep .4
probe motion 54 550 350
sleep .45
probe up 54
sleep .5
stacked_pair
probe down 55 500 350
sleep .4
probe motion 55 620 360
sleep .3
probe up 55
sleep .6
stacked_pair || { echo "DIAG sideways: $(cards)"; false; }
echo 'PASS: short sideways travel neither reorders nor releases the member'
test "$(probe releaseRuntime)" = true
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: release returns both windows after the stack gestures'
