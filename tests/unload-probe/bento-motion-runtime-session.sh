#!/usr/bin/env bash
# CARD-LIFECYCLE.md §5, §8 and §12 on the display that can own cards, in
# motion. A pair's panes keep gliding to their places when a hidden card
# closes as they go; a pane that yields to an arrival fades where it stood
# rather than vanishing; and when the layout falls to one pane, that pane grows
# from where it stood into the Active place rather than jumping there.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: bento motion: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId, bento: .desktopStage.active, apps: [.applications[] | {title, windowId, hasCard, selected}]}')"
}
now() { date +%s%3N; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe contactObserve
probe contactStart >/dev/null
client colouredCompanion "Neighbour" "c03a3a" 700 500
client colouredCompanion "Aside" "2e8b57" 560 420
sleep .8
neighbour=$(probe windowIdByCaption "Neighbour")
aside=$(probe windowIdByCaption "Aside")
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
main=$(kad workspaceContext | jq -r --arg n "$neighbour" --arg a "$aside" \
    '[.applications[] | select(.windowId != $n and .windowId != $a)][0].windowId')
probe contactFocus >/dev/null
sleep .5
# The neighbour is the card used before the Active one, so carried to the
# left edge the Active card pairs with it and leaves the third a hidden card.
probe activateWindowId "$neighbour" >/dev/null
sleep .4
probe contactFocus >/dev/null
sleep .5
report start
check "the Active card" context --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'

# The pair glides into its panes. The hidden card closing as it goes takes
# nothing from the glide: the panes are still on their way once it is gone.
bounds=$(probe windowGeometry "$main")
x=$(jq '.x+.width/2|floor' <<<"$bounds"); y=$(jq '.y+40|floor' <<<"$bounds")
probe pointer "$x" "$y"
client armMove
probe down 61 "$x" "$y"
sleep .15
probe motion 61 650 400; sleep .15
probe motion 61 5 400; sleep .3
start=$(now)
probe up 61
client closeCompanion "Aside"
for sample in {1..20}; do
    if kad workspaceContext | jq -e --arg id "$aside" '[.applications[] | select(.windowId == $id)] | length == 0' >/dev/null; then break; fi
    sleep .01
done
glide=$(kad nativeCarryState | jq -c '[.bentoMotion[] | {window, arrival, started, rect, target}]')
echo "glide $(( $(now) - start )) ms after the pair, once the hidden card closed: $glide"
check "the panes still glide once a hidden card closes" jq -e 'map(select(.rect != .target)) | length > 0' <<<"$glide"
sleep 1
report paired
check "a Bento pair" context '.desktopStage.active'
check "the hidden card is gone" context --arg id "$aside" '[.applications[] | select(.windowId == $id)] | length == 0'

# An arrival at the full pair takes one pane's place; the pane that yields
# fades where it stood as the arrival takes its place.
client colouredCompanion "Arrival" "1e6fc8" 600 450
fading=()
for sample in {1..40}; do
    fading+=("$(kad nativeCarryState | jq -c '.yieldingPanes[]')")
    sleep .02
done
printf '%s\n' "${fading[@]}" | grep . | sed 's/^/yielding /'
check "the pane that yields fades where it stood" \
    jq -se 'map(select(.opacity > 0 and .opacity < 1)) | length > 0' < <(printf '%s\n' "${fading[@]}" | grep .)
sleep 1
report arrived
arrival=$(probe windowIdByCaption "Arrival")
yielded=$(printf '%s\n' "${fading[@]}" | grep . | head -1 | jq -r '.window')
check "the yielded pane is one of the pair" test -n "$yielded"
check "the yielded pane is a card" context --arg id "$yielded" 'first(.applications[] | select(.windowId == $id)) | .hasCard'
check "the layout shows the arrival" context '.desktopStage.active'
survivor=$main
[[ $yielded == "$main" ]] && survivor=$neighbour

# The arrival closes and the layout falls to one pane, which becomes the
# Active card: it grows from where it stood into the Active place.
client closeCompanion "Arrival"
growth=()
for sample in {1..30}; do
    growth+=("$(kad nativeCarryState | jq -c 'select(.dropSettling) | {dropWindow, dropStarted, dropRect, dropTarget}')")
    sleep .02
done
printf '%s\n' "${growth[@]}" | grep . | sed 's/^/last pane /'
check "the last pane grows from its pane into the Active place" \
    jq -se --arg id "$survivor" 'map(select(.dropWindow == $id and .dropRect != .dropTarget)) | length > 0' \
    < <(printf '%s\n' "${growth[@]}" | grep .)
sleep 1
report ended
check "the last pane is the Active card" context --arg id "$survivor" \
    '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id and (.desktopStage.active | not)'
check "the arrival is gone" context --arg id "$arrival" '[.applications[] | select(.windowId == $id)] | length == 0'
check "no ownership violations" context '(.ownershipViolations // []) | length == 0'

test "$(probe releaseRuntime)" = true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .3
probe contactDrop
kill "$client_pid"; wait "$client_pid" 2>/dev/null

if ((failures)); then echo "FAIL: bento motion: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a pair glides on as a hidden card closes, a yielding pane fades, and the last pane grows into the Active place'
