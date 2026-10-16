#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Connected, on one 1920x1080 monitor with no
# touchscreen, a 24 px status bar along its top and a 56 px dock along its
# bottom. It is Connected by its size, though its panels leave a 1900x980
# stage, below the room that caps a work area at two panes, and it shows as
# many panes as minimum sizes allow. The layout sizes its panes, sharing the
# width evenly within minimum sizes; an edge says only where a card goes. A
# pick pairs evenly with the card in use, a third pick takes a third column
# and a fourth joins the shortest column, the panes already there keeping
# their places. Past minimum sizes the layout is laid out whole, a card
# snapped to a side keeping that side, and that card is never the pane that
# leaves for Spread. Every check reads where each pane stands and its size.
# Every check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected sizes: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        displays: [.displayContext.displays[] | {name, role, sizing, bentoActive}],
        apps: [.applications[] | {title, hasCard, pane, focused, minimized}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client" and .normal) | {caption, x, y, width, height, hidden})')"
}
# Meta+S by evdev code; Spread's own keys move along the row and open a card.
spread_key() { probe key 31 125 >/dev/null; sleep 1; }
open_selected() { probe key 28 0 >/dev/null; sleep 1.2; }
selected() { [[ $(kad workspaceContext | jq -r '.cardStage.selectedCardId') == "$1" ]]; }
# The row has two ends, so the keys walk to one end and then to the other.
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
pick() { spread_key; select_card "$1" || echo "could not select $1"; open_selected; }
is_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | (.hasCard | not) and (.minimized | not)'; }
is_card() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard'; }
panes() { context --argjson n "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))] | length == $n'; }
layout_shown() { context '.cardStage.presentation == "bento" and ([.displayContext.displays[] | select(.bentoActive)] | length == 1)'; }
rect() { probe windowFacts | jq -c --arg id "$1" 'first(.[] | select(.id == $id)) | {x, y, width, height}'; }
same_rect() { [[ $(rect "$1") == "$2" ]]; }
# The window's frame is [x, y, width, height], to the pixel KWin rounds to.
at() {
    probe windowFacts | jq -e --arg id "$1" --argjson r "$2" 'first(.[] | select(.id == $id))
        | [.x - $r[0], .y - $r[1], .width - $r[2], .height - $r[3]] | map(fabs) | max <= 1'
}
geometry() { probe windowFacts | jq -e --arg id "$1" "first(.[] | select(.id == \$id)) | $2"; }
# The client brings the panels a 1080p desktop has: a bar along the top and a
# dock along the bottom, each keeping its height from the work area.
start_client() {
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    sleep 1
    client topBarSurface Virtual-0 24
    client dockSurface 56
    sleep .6
}
open_window() { client titledCompanion "$1" "$2" "$3"; sleep .5; }
load_effect() {
    # The client's own window sleeps, so only the named windows take part.
    probe minimizeWindow "$(idOf unload-client)" true >/dev/null
    sleep .4
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1.2
}
end_pass() {
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .8
    check "$1: switched off, every window is an ordinary window again" \
        bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .normal and .caption != \"unload-client\")) | length >= 2 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
    kill "$client_pid" 2>/dev/null || true
    wait "$client_pid" 2>/dev/null || true
    sleep .5
}
# Carries a window by a press inside it and lets it go at x y, as a person's
# mouse does: the release names a side, and the upper half of an edge puts
# the card beside, a full-height column at that edge.
snap() {
    local id=$1 title=$2 x y
    read -r x y <<<"$(probe windowGeometry "$id" | jq -r '[.x+.width/2,.y+.height/2] | map(floor) | @tsv')"
    # Focus goes to the window carried, not the client's own.
    probe activateWindowId "$id" >/dev/null
    sleep .3
    probe pointer "$x" "$y" >/dev/null
    client armMoveOf "$title"
    probe contactButton true >/dev/null
    sleep .12
    probe contactMotion "$3" "$4" >/dev/null
    sleep .3
    probe contactButton false >/dev/null
    sleep 1
}
for _ in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 960 540

# The stage is the work area the panels leave, 1920x1000 from y 24, less the
# layout's 10 px margin: 1900x980 from (10, 34), with 14 px between panes.
# Pass one: windows small enough that the layout grows with every pick.
start_client
for title in 'Card A' 'Card B' 'Card C' 'Card D'; do open_window "$title" 500 400; done
load_effect
a=$(idOf 'Card A') b=$(idOf 'Card B') c=$(idOf 'Card C') d=$(idOf 'Card D')
probe activateWindowId "$a" >/dev/null
sleep .8
report start
check 'by size: the monitor is sized Connected, its panels notwithstanding' \
    bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce displaySizing Virtual-0) == connected ]]"
check 'start: the card in use is Active' context --arg id "$a" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
pick "$b"
report paired
check 'pair: the layout shows' layout_shown
check 'pair: two panes' panes 2
check 'pair: the card in use takes the left half, top to bottom of the stage' at "$a" '[10, 34, 943, 980]'
check 'pair: the pick takes the right half, the same size' at "$b" '[967, 34, 943, 980]'
pick "$c"
report thirds
check 'third: three panes, where the stage alone would cap at two' panes 3
check 'third: the first card stays at the left, a third wide' at "$a" '[10, 34, 626, 980]'
check 'third: the second stays beside it, a third wide' at "$b" '[650, 34, 619, 980]'
check 'third: the pick takes the third column at the right' at "$c" '[1284, 34, 626, 980]'
slot_a=$(rect "$a") slot_b=$(rect "$b")
pick "$d"
report fourth
check 'fourth: four panes' panes 4
check 'fourth: the first card did not move' same_rect "$a" "$slot_a"
check 'fourth: the second card did not move' same_rect "$b" "$slot_b"
check 'fourth: the third keeps the top of its column' at "$c" '[1284, 34, 626, 483]'
check 'fourth: the pick joins that column, under it' at "$d" '[1284, 531, 626, 483]'
end_pass even

# Pass two: a card too wide for half the stage. The pair cannot hold it in
# even halves, so it is laid out whole, the wide card in the larger share,
# and a third pick still finds room, which a stage capped at two would not.
start_client
open_window 'Wide' 1000 400
open_window 'Narrow A' 500 400
open_window 'Narrow B' 500 400
load_effect
w=$(idOf 'Wide') na=$(idOf 'Narrow A') nb=$(idOf 'Narrow B')
probe activateWindowId "$w" >/dev/null
sleep .8
pick "$na"
report wide-pair
check 'wide pair: two panes' panes 2
check 'wide pair: the wide card takes the larger share, at the left' at "$w" '[10, 34, 1171, 980]'
check 'wide pair: the pick takes the rest' at "$na" '[1195, 34, 715, 980]'
pick "$nb"
report wide-three
check 'wide three: three panes' panes 3
check 'wide three: the wide card keeps a share it fits' at "$w" '[10, 34, 1133, 980]'
check 'wide three: the narrow cards share the column beside it' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e --arg a '$na' --arg b '$nb' '[.[] | select(.id == \$a or .id == \$b)]
        | length == 2 and all(((.x - 1157) | fabs) <= 1 and ((.width - 753) | fabs) <= 1 and ((.height - 483) | fabs) <= 1)
        and ([.[].y] | sort | ((.[0] - 34) | fabs) <= 1 and ((.[1] - 531) | fabs) <= 1)'"
end_pass wide

# Pass three: cards so large that two fill the stage. One is snapped beside
# on the left and used no more; the pick after it is used later. A third,
# fitting no column beside them, takes the place of a pane used longest ago,
# and the snapped card, though used longest ago, is never that pane.
start_client
for title in 'Snapped' 'Earlier' 'Later'; do open_window "$title" 900 700; done
probe contactObserve >/dev/null
probe contactStart >/dev/null
load_effect
s=$(idOf 'Snapped') e=$(idOf 'Earlier') l=$(idOf 'Later')
probe activateWindowId "$s" >/dev/null
sleep .8
snap "$s" 'Snapped' 5 250
report snapped
check 'snap: beside on the left, alone, the card takes the left half' at "$s" '[10, 34, 943, 980]'
check 'snap: it stands as the layout' layout_shown
pick "$e"
report filled
check 'fill: the pick takes the half left open' at "$e" '[967, 34, 943, 980]'
check 'fill: the snapped card did not move' at "$s" '[10, 34, 943, 980]'
pick "$l"
report kept
check 'kept: two panes, no third holding these minimums' panes 2
check 'kept: the snapped card stays, at its side and its size' at "$s" '[10, 34, 943, 980]'
check 'kept: the pick takes the other pane, its place and its size' at "$l" '[967, 34, 943, 980]'
check 'kept: that card waits in Spread' is_card "$e"
end_pass kept

# Pass four: the third card fits no column the layout holds, so the layout is
# laid out whole, and the card snapped on the left keeps the left edge, a
# full-height column of its own.
start_client
open_window 'Snapped' 500 400
open_window 'Earlier' 500 400
open_window 'Large' 800 600
probe contactObserve >/dev/null
probe contactStart >/dev/null
load_effect
s=$(idOf 'Snapped') e=$(idOf 'Earlier') g=$(idOf 'Large')
probe activateWindowId "$s" >/dev/null
sleep .8
snap "$s" 'Snapped' 5 250
pick "$e"
report side-filled
check 'side: the snapped card holds the left half' at "$s" '[10, 34, 943, 980]'
check 'side: the pick holds the right half' at "$e" '[967, 34, 943, 980]'
pick "$g"
report side-auto-fill
check 'auto-fill: three panes, none left for Spread' panes 3
check 'auto-fill: the snapped card keeps the left edge, full height' at "$s" '[10, 34, 559, 980]'
check 'auto-fill: the large card holds its minimum beside it' at "$g" '[583, 34, 813, 980]'
check 'auto-fill: the other card holds its minimum at the right' at "$e" '[1410, 34, 500, 980]'
end_pass side

if (( failures > 0 )); then
    echo "FAIL: connected sizes: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: a Connected 1080p monitor with panels pairs evenly, keeps its panes in place as picks arrive, and keeps a snapped card at its side'
