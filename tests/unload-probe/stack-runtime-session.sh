#!/usr/bin/env bash
# CARD-LIFECYCLE.md §9: an ordinary stack releases a member pulled down out of
# it, and a held member let go over its own Stack rejoins it however high it
# was raised or however far it strayed sideways. This drives each with real
# input and requires each to leave the others' answers alone.
set -euo pipefail
trap 'echo "FAIL: stack runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
source "$(dirname "${BASH_SOURCE[0]}")/spread-carry.bash"
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
# Form a two-card stack: one card carried onto the other, resting there.
carry_onto_centre 51
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
# Held and raised well past where rising once released it, the member still
# rejoins: rising no longer takes a card out of its Stack.
probe down 53 500 350
sleep .4
probe motion 53 500 120
sleep .3
probe up 53
sleep .6
stacked_pair || { echo "DIAG raised: $(cards)"; false; }
echo 'PASS: a held member raised and let go rejoins the stack'
# §9: pulled down out of the stack, the member is released into the Spread
# beside it. Neither window is minimized, lost, or handed to Plasma.
probe down 56 500 350
sleep .1
probe motion 56 500 362
sleep .05
probe motion 56 500 540
probe up 56
sleep .6
separate_cards || { echo "DIAG release: $(cards)"; false; }
kad workspaceContext | jq -e '.cardStage.active'
echo 'PASS: a member pulled down out of the stack is released into the Spread'
# §9: a member held and moved sideways stays over its own Stack, so it is let
# go back into it rather than leaving. Rebuild the stack and stray from it.
kad showActive
sleep .5
kad showCardLine
sleep .5
carry_onto_centre 54
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
echo 'PASS: sideways travel keeps a held member in its Stack'
# A Stack is a ring: from one of its open cards, side steps go round it and
# never leave it, though another card stands beside it in the row.
client titledCompanion "Ring outsider probe" 500 400
chosen() { kad workspaceContext | jq -c '[.applications[] | select(.hasCard and .selected)][0] | {w: .windowId, n: .stackSize, p: .stackPosition}'; }
three_cards() { kad workspaceContext | jq -e '[.applications[] | select(.hasCard)] | length == 3' >/dev/null; }
for attempt in {1..25}; do three_cards && break; sleep .2; done
three_cards || { echo "DIAG outsider: $(kad workspaceContext | jq -c '[.applications[] | {c: .hasCard, n: .stackSize, m: .minimized}]')"; false; }
for k in 105 106 106; do
    [[ $(chosen | jq '.n') == 2 ]] && break
    probe key "$k" 0
    sleep .4
done
chosen | jq -e '.n == 2'
kad showActive
sleep .6
start=$(chosen)
for meta_key in 106 106 106 105 105 105; do
    probe key "$meta_key" 125
    sleep .4
    chosen | jq -e '.n == 2' >/dev/null || { echo "DIAG ring left the Stack: $(chosen)"; false; }
done
test "$(chosen | jq '.p')" = "$(jq '.p' <<<"$start")"
probe key 106 125
sleep .4
test "$(chosen | jq '.p')" != "$(jq '.p' <<<"$start")"
echo 'PASS: an open card steps round its Stack and never out of it'
test "$(probe releaseRuntime)" = true
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
echo 'PASS: release returns both windows after the stack gestures'
