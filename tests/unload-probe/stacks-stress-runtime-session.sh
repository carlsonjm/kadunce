#!/usr/bin/env bash
# The stability bar: four Stacks of four cards, used as a person uses them,
# without a sneeze. On one 2560x1440 monitor sized Connected, four panes each
# hold a Stack of four, built by resting a pane on another. Then, twice over:
# Meta+Tab steps every Stack round to its front again, Spread opens over the
# layout and closes, Meta+B ends the layout into cards and lays it out again,
# the second time from Spread,
# a card at the back of a Stack is called to its pane, and a card closed from
# a Stack leaves the rest of it whole. After every step the card book holds
# every card, every window is a pane or a card, and every Stack is whole.
# Switched off, all sixteen are ordinary windows on the display. Every check is
# reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: stacks stress: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, hasCard, pane, paneStack, stackId, stackSize, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, x, y, width, height, hidden})')"
}
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
spread_key() { probe key 31 125 >/dev/null; sleep 1; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.2; }
selected() { [[ $(kad workspaceContext | jq -r '.cardStage.selectedCardId') == "$1" ]]; }
select_card() {
    local id=$1 key
    for key in 105 106; do
        for _ in {1..14}; do
            selected "$id" && return 0
            probe key "$key" 0 >/dev/null
            sleep .3
        done
    done
    selected "$id"
}
pick() { select_card "$1" || echo "could not select $1"; open_selected; }
is_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | (.hasCard | not) and (.minimized | not)'; }
is_card() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard'; }
panes() { context --argjson n "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))] | length == $n'; }
layout_shown() { context '.cardStage.presentation == "bento" and ([.displayContext.displays[] | select(.bentoActive)] | length == 1)'; }
# The cards waiting behind a pane, front to back, as one list of ids.
behind_is() { context --arg id "$1" --argjson ids "$2" 'first(.applications[] | select(.windowId == $id)) | .paneStack == $ids'; }
behind_count() { context --arg id "$1" --argjson n "$2" 'first(.applications[] | select(.windowId == $id)) | (.paneStack | length) == $n'; }
one_stack() { context --argjson ids "$1" '[.applications[] | select(.windowId as $w | $ids | index($w)) | .stackId] | (length == ($ids | length) and (unique | length) == 1)'; }
# A pane carried by a press inside it onto another pane, let go after `rest`
# seconds there; what the carry would do is kept to read afterwards.
drag_onto() {
    local id=$1 title=$2 to=$3 rest=$4 x y tx ty
    read -r x y <<<"$(probe windowGeometry "$id" | jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv')"
    read -r tx ty <<<"$(probe windowGeometry "$to" | jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv')"
    probe activateWindowId "$id" >/dev/null
    sleep .3
    probe pointer "$x" "$y" >/dev/null
    client armMoveOf "$title"
    probe contactButton true >/dev/null
    sleep .12
    probe contactMotion "$((x + 40))" "$((y + 20))" >/dev/null
    sleep .1
    probe contactMotion "$tx" "$ty" >/dev/null
    sleep "$rest"
    kad nativeCarryState >"$XDG_RUNTIME_DIR/carry"
    echo "state drag-$id $(jq -c '{stackOnto, stackLean, destinationRect}' "$XDG_RUNTIME_DIR/carry")"
    probe contactButton false >/dev/null
    sleep 1.2
}
aimed() { jq -e --arg id "$1" '.stackOnto == $id' "$XDG_RUNTIME_DIR/carry"; }
# Every window is a pane or a card, never an ordinary window left over.
all_held() {
    context '[.applications[] | select(.title != "unload-client") | select((.hasCard | not) and (.pane | not) and (.minimized | not))] | length == 0'
}

book_clean() { [[ -z $(kad cardBookMismatches | tr -d '[:space:][]') ]]; }
stack_size_of() { kad workspaceContext | jq -r --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .stackSize'; }
# Every pane shows the front of a Stack of `n`, with `n - 1` waiting behind.
stacks_of() {
    context --argjson n "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))]
        | length == 4 and all((.paneStack | length) == $n - 1)'
}
# The cards of every Stack, front first, as one sorted list of id lists.
stack_sets() {
    kad workspaceContext | jq -c '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))
        | ([.windowId] + .paneStack) | sort] | sort'
}
# A window opened while the layout shows joins it; one that stays a card is
# picked into it from Spread.
open_window() {
    client titledCompanion "$1" 500 400
    sleep 1
    w[$2]=$(idOf "$1")
    if ! is_pane "${w[$2]}" >/dev/null 2>&1; then
        echo "state opened-as-card-$1"
        spread_key
        pick "${w[$2]}"
    fi
}
step_key() { probe activateWindowId "$1" >/dev/null; sleep .4; probe key 15 125 >/dev/null; sleep 1.2; }
front_of() {
    kad workspaceContext | jq -r --arg id "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))
        | select(.windowId == $id or (.paneStack | index($id)))][0].windowId'
}
# What every step must leave: the book whole, nothing loose, the Stacks whole.
steady() {
    local tag=$1 n=$2
    report "$tag"
    check "$tag: the card book holds every card" book_clean
    check "$tag: every window is a pane or a card" all_held
    check "$tag: four panes, each the front of a Stack of $n" stacks_of "$n"
}

for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 1280 720
probe contactObserve >/dev/null
probe contactStart >/dev/null
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for n in {1..4}; do client titledCompanion "S$n" 500 400; sleep .3; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
declare -A w
for n in {1..4}; do w[$n]=$(idOf "S$n"); done
probe activateWindowId "${w[1]}" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
check 'start: the monitor is sized Connected' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce displaySizing Virtual-0) == connected ]]"
for n in 2 3 4; do spread_key; pick "${w[$n]}"; done
report four
check 'four: four panes' panes 4

# Each pane gets three more cards, each opened and rested on the pane's front.
declare -A front
for p in 1 2 3 4; do front[$p]=${w[$p]}; done
n=4
for p in 1 2 3 4; do
    for _ in 1 2 3; do
        n=$((n + 1))
        open_window "S$n" "$n"
        drag_onto "${w[$n]}" "S$n" "${front[$p]}" .9
        front[$p]=${w[$n]}
    done
done
steady built 4
built=$(stack_sets)
echo "state stacks $built"
check 'built: sixteen cards in four Stacks of four' \
    bash -c "jq -e 'length == 4 and all(length == 4) and (flatten | unique | length == 16)' <<<'$built'"

for round in 1 2; do
    # Meta+Tab four times steps a Stack all the way round to its front.
    for p in 1 2 3 4; do
        first=$(front_of "${front[$p]}")
        for _ in 1 2 3 4; do step_key "$(front_of "${front[$p]}")"; done
        check "round $round: Stack $p stepped round comes back to its front" test "$(front_of "${front[$p]}")" = "$first"
    done
    steady "round $round stepped" 4
    check "round $round stepped: every Stack holds the same cards" test "$(stack_sets)" = "$built"

    # Spread over the layout, and back.
    spread_key
    check "round $round: Meta+S opens Spread" presents cardLine
    probe key 1 0 >/dev/null
    sleep 1.2
    steady "round $round spread" 4
    check "round $round spread: every Stack holds the same cards" test "$(stack_sets)" = "$built"

    # Meta+B ends the layout into cards, and lays it out again.
    probe key 48 125 >/dev/null
    sleep 1.8
    report "round $round ended"
    check "round $round: Meta+B ends the layout" panes 0
    check "round $round ended: the card book holds every card" book_clean
    check "round $round ended: every window is a card" all_held
    for p in 1 2 3 4; do
        check "round $round ended: Stack $p is still four cards" test "$(stack_size_of "${front[$p]}")" = 4
    done
    # The second time, Meta+B in Spread closes it and lays every card out.
    if [[ $round == 2 ]]; then
        spread_key
        check "round $round ended: Meta+S opens Spread" presents cardLine
    fi
    probe key 48 125 >/dev/null
    sleep 2.4
    steady "round $round again" 4
    check "round $round again: every Stack holds the same cards" test "$(stack_sets)" = "$built"

    # A card at the back of a Stack, called up, fills its pane.
    for p in 1 2 3 4; do
        back=$(kad workspaceContext | jq -r --arg f "$(front_of "${front[$p]}")" \
            'first(.applications[] | select(.windowId == $f)) | .paneStack[-1]')
        probe activateWindowId "$back" >/dev/null
        sleep 1.2
        check "round $round: the back card of Stack $p, called, fills its pane" is_pane "$back"
    done
    steady "round $round called" 4
    check "round $round called: every Stack holds the same cards" test "$(stack_sets)" = "$built"
done

# A card closed from the back of each Stack leaves the rest of it whole.
for p in 1 2 3 4; do
    f=$(front_of "${front[$p]}")
    back=$(kad workspaceContext | jq -r --arg f "$f" 'first(.applications[] | select(.windowId == $f)) | .paneStack[-1]')
    caption=$(probe windowFacts | jq -r --arg id "$back" 'first(.[] | select(.id == $id)) | .caption')
    client closeCompanion "$caption"
    sleep 1.2
done
steady closed 3
check 'closed: twelve cards left in four Stacks of three' \
    bash -c "jq -e 'length == 4 and all(length == 3)' <<<'$(stack_sets)'"
echo "state warnings $(grep -ac 'Kadunce.*\(publishing\|unsettled\|refused\|mismatch\)' "$(dirname "$XDG_RUNTIME_DIR")/session.log" 2>/dev/null)"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
check 'switched off: every window is an ordinary window again' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 12 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
if (( failures > 0 )); then
    echo "FAIL: stacks stress: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: four Stacks of four step, open Spread, end and lay out again, call and close cards with every card held and every Stack whole'
