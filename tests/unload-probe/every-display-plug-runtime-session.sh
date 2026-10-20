#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display, as a tablet meets a monitor: Virtual-0
# at scale 2 is small, primary and holds the dock; Virtual-1 is plugged in
# later. Plugged in, the monitor takes every card of the display with the dock,
# the one in use shown there and the rest in its Spread, and the small display
# keeps none. A card sent back goes alone. Unplugged, the cards come home;
# back within moments, each card stands with the display holding it and none
# is let go; plugged in again after a while, every card moves to the monitor
# again. Every check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: every display plug: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{displays: [.displayContext.displays[] | {name, primary, sizing, holdsCards,
            stage: (.cardStage // {} | {presentation, activeCardId})}],
        apps: [.applications[] | {title, windowId, output, hasCard, pane, stage, focused, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, output, x, y, width, height, hidden})')"
}
plug() { kscreen-doctor output.Virtual-1.enable >/dev/null 2>&1; sleep 3; }
unplug() { kscreen-doctor output.Virtual-1.disable >/dev/null 2>&1; sleep 2.5; }
cards() { context --argjson n "$1" '[.applications[] | select(.hasCard)] | length == $n'; }
# Every card is a card of the display named, and its window stands there.
all_on() { context --arg d "$1" '[.applications[] | select(.hasCard)] | length > 0 and all(.stage == $d and .output == $d)'; }
card_of() { context --arg d "$1" --arg id "$2" 'first(.applications[] | select(.windowId == $id)) | .hasCard and .stage == $d and .output == $d'; }
each_on_its_own() { context '[.applications[] | select(.hasCard and .stage != .output)] | length == 0'; }
shows_on() { context --arg d "$1" --arg id "$2" 'first(.displayContext.displays[] | select(.name == $d)) | .cardStage
    | .presentation == "active" and .activeCardId == $id'; }
holds_none() { context --arg d "$1" '[.applications[] | select(.hasCard and .stage == $d)] | length == 0'; }
for _ in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
kscreen-doctor output.Virtual-1.disable >/dev/null 2>&1
kscreen-doctor output.Virtual-0.scale.2 >/dev/null 2>&1
sleep 1
kwriteconfig6 --file kaduncerc --group Spread --key Display Virtual-0
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client bottomSurface 48 >/dev/null
for title in 'Tab A' 'Tab B' 'Tab C'; do client titledCompanion "$title" 500 400; sleep .5; done
probe minimizeWindow "$(idOf unload-client)" true >/dev/null
sleep .4
ta=$(idOf 'Tab A') tb=$(idOf 'Tab B') tc=$(idOf 'Tab C')
probe activateWindowId "$tc" >/dev/null
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
probe activateWindowId "$tc" >/dev/null
sleep 1
report start
check 'start: every card is the small display own' all_on Virtual-0
check 'start: four cards, the minimized one asleep' cards 4
check 'start: the small display shows the card in use' shows_on Virtual-0 "$tc"

# Plugged in, the monitor takes every card, and shows the one in use.
plug
report plugged
check 'plugged: every card moved to the monitor' all_on Virtual-1
check 'plugged: no card was let go' cards 4
check 'plugged: the small display holds no card' holds_none Virtual-0
check 'plugged: the monitor shows the card in use' shows_on Virtual-1 "$tc"

# One card sent back goes alone.
probe sendCaptionToOutput 'Tab A' Virtual-0 >/dev/null
sleep 2
report sent
check 'sent: the card sent back is the small display own' card_of Virtual-0 "$ta"
check 'sent: the others stay on the monitor' card_of Virtual-1 "$tb"
check 'sent: the card in use stays on the monitor' card_of Virtual-1 "$tc"
check 'sent: every card is a card of the display it stands on' each_on_its_own

# Unplugged, every card comes home; back within moments, nothing is thrown
# about: each card is a card of the display it stands on.
unplug
report unplugged
check 'unplugged: every card is the small display own' all_on Virtual-0
check 'unplugged: no card was let go' cards 4
plug
report returned
check 'returned: every card is a card of the display it stands on' each_on_its_own
check 'returned: no card was let go' cards 4

# Away for a while, the monitor plugged in again takes every card again.
unplug
sleep 9
plug
report replugged
check 'replugged: every card moved to the monitor' all_on Virtual-1
check 'replugged: no card was let go' cards 4
check 'replugged: the small display holds no card' holds_none Virtual-0

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .8
report released
check 'switched off: every window is an ordinary window on a display that exists' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\" and .caption != \"\")) | length == 3 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
kill "$client_pid" 2>/dev/null || true
if (( failures > 0 )); then
    echo "FAIL: every display plug: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: a display plugged in takes the cards of the display with the dock'
