#!/usr/bin/env bash
# INPUT.md § Bento, by the keys alone, on two monitors with no touchscreen, as
# a desk is used: Virtual-0 is small and left of Virtual-1, which is large,
# sized Connected and chosen for Spread. With a layout of two panes there,
# Table opens and closes, then Meta+B ends the layout into cards, the one in
# use Active; Meta+B again lays out every card open there; and Meta+S
# opens Spread over the layout and closes it again. Every check is reported,
# so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected bento key: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, output, hasCard, pane, paneStack, stackId, focused, minimized}]}')"
    echo "stages $1 $(kad outputStageState | tr '\n' ' ')"
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
large_panes() { kad workspaceContext | jq '[.applications[] | select(.pane and (.hasCard | not) and .output == "Virtual-1")] | length'; }
panes_are() { [[ $(large_panes) == "$1" ]]; }
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
for title in 'Small' 'Mail' 'Wallet' 'Browser'; do client titledCompanion "$title" 500 400; sleep .5; done
probe sendCaptionToOutput 'Small' Virtual-0
for title in 'Mail' 'Wallet' 'Browser'; do probe sendCaptionToOutput "$title" Virtual-1; done
sleep .5
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
mail=$(idOf 'Mail') wallet=$(idOf 'Wallet') browser=$(idOf 'Browser')
probe activateWindowId "$mail" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
spread_key
pick "$wallet"
report start
check 'start: the large display shows a layout of two panes' panes_are 2

# Meta+S over the layout opens Spread, and Escape goes back to it.
spread_key
report spread
check 'spread: Meta+S opens Spread over the layout' presents cardLine
probe key 1 0 >/dev/null
sleep 1.2
report spread-back
check 'spread: Escape goes back to the layout' panes_are 2

# Table opens and closes by its key; then Meta+B ends the layout.
probe key 17 125 >/dev/null
sleep 1.2
report table
probe key 17 125 >/dev/null
sleep 1.2
report table-closed
check 'table: Meta+W twice leaves the layout as it was' panes_are 2
probe key 48 125 >/dev/null
sleep 1.6
report ended
check 'end: Meta+B ends the layout' panes_are 0
check 'end: its windows are cards, one of them Active' context '.cardStage.presentation == "active" and .cardStage.selectedCardId != null'
probe key 48 125 >/dev/null
sleep 1.6
report paired
check 'fill: Meta+B again lays out every open card of the display' panes_are 3
spread_key
report spread-again
check 'spread: Meta+S still opens Spread' presents cardLine
check 'spread: the panes are cards of their own there, no two in one Stack' context '[.applications[] | select(.output == "Virtual-1" and .stackId != null) | .stackId] | length == (unique | length)'
probe key 1 0 >/dev/null
sleep 1.2
report final
check 'spread: and Escape goes back to the layout' panes_are 3

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length == 4 and all(.output != \"\" and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: connected bento key: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: Meta+B ends and remakes a layout on Connected, and Meta+S opens Spread over it, Table or no Table'
