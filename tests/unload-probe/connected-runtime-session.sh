#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11 on one 2560x1440 monitor with no touchscreen, sized
# Connected by its room. Spread shows the chosen card whole between its
# neighbours' edges, as on Solo, and every window is a card of its own, a
# layout's panes included. A picked card joins the layout beside the card in
# use and the layout grows; full, the pane used longest ago leaves and becomes
# a card; a picked pane opens the layout on it. A Stack fills one pane, and
# Meta+Tab steps it in place. The per-display choice is kept in kaduncerc, and
# Solo sizing keeps each pane its own card; a choice still saved as "active",
# Solo's earlier name, reads and is kept as Solo. Every check is reported, so a
# failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        displays: [.displayContext.displays[] | {name, role, sizing, sizingChosen, bentoActive}],
        apps: [.applications[] | {title, hasCard, stackId, stackSize, focused, minimized, spreadRect}]}')"
    echo "facts $1 $(probe windowFacts | jq -c 'map(select(.class == "unload-client") | {caption, x, y, width, height, hidden})')"
}
presents() { context --arg p "$1" '.cardStage.presentation == $p'; }
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
pick() { select_card "$1" || echo "could not select $1"; open_selected; }
is_pane() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | (.hasCard | not) and (.minimized | not)'; }
both_panes() { is_pane "$1" && is_pane "$2"; }
is_card() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .hasCard'; }
panes() { context --argjson n "$1" '[.applications[] | select(.title != "unload-client" and (.hasCard | not) and (.minimized | not))] | length == $n'; }
layout_shown() { context '.cardStage.presentation == "bento" and ([.displayContext.displays[] | select(.bentoActive)] | length == 1)'; }
focused() { context --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .focused'; }
# Every card stands alone: no window of the layout hides inside another.
each_its_own() {
    context '[.applications[] | select(.hasCard and (.minimized | not))] as $c
        | ($c | length) >= 2 and all($c[]; .stackSize == 1) and ([$c[].stackId] | unique | length) == ($c | length)'
}
# Spread as on Solo: the selected card stands whole and centred, the
# large card of the row.
one_whole() {
    context '.cardStage.selectedCardId as $s
        | first(.applications[] | select(.windowId == $s)) | .spreadRect as $r
        | ($r.x + $r.width / 2 - 1280) as $d
        | $r.x >= 0 and $r.x + $r.width <= 2560 and $r.width > 1280 and $d < 8 and $d > -8'
}
selected_is() { context --arg id "$1" '.cardStage.selectedCardId == $id'; }
selected_not() { context --arg id "$1" '.cardStage.selectedCardId != $id'; }
rect() { probe windowFacts | jq -c --arg id "$1" 'first(.[] | select(.id == $id)) | {x, y, width, height}'; }
same_rect() { [[ $(rect "$1") == "$2" ]]; }
spread_left=0
# Where a person presses a card of Spread: the middle of the part of it in
# view, or nothing while it is out of view.
seen_at() {
    kad workspaceContext | jq -r --arg id "$1" --argjson left "$spread_left" 'first(.applications[] | select(.windowId == $id)) | .spreadRect
        | select(. != null) | [([.x, $left] | max), ([.x + .width, $left + 2560] | min)] as [$l, $r]
        | select($r - $l > 120) | "\((($l + $r) / 2) | floor) \((.y + .height / 2) | floor)"'
}
entry_of() { kad workspaceContext | jq -r --arg id "$1" 'first(.applications[] | select(.windowId == $id)) | .entry'; }
carry_aim() { kad nativeCarryState | jq -r '"\(.carryAim) \(.carryIndex)"'; }
# A person's mouse carries one card onto another in Spread and lets go. Spread
# shows one whole card and its neighbours' edges, so the card is carried as a
# person does: pulled down, the row zooms out. Where all of it shows, the card
# goes straight over the other; where it does not, the card leans the row
# along until the other comes under it, then rests in the middle until it
# would join (CarryRow.h).
carry_onto() {
    local from=$1 to=$2 x y h e entries t items scale width pitch mid x0 dx aim at k
    select_card "$from" >/dev/null 2>&1
    sleep .4
    read -r x y < <(seen_at "$from")
    width=$(kad workspaceContext | jq -r --arg id "$from" 'first(.applications[] | select(.windowId == $id)) | .spreadRect.width')
    h=$(entry_of "$from")
    e=$(entry_of "$to")
    entries=$(kad workspaceContext | jq '[.applications[] | .entry | select(. != null and . >= 0)] | max + 1')
    items=$((entries - 1))
    t=$(( e > h ? e - 1 : e ))
    mid=$((spread_left + 1280))
    probe pointer "$x" "$y"
    probe button true
    sleep .4
    probe pointer "$((x + 30))" "$y"
    sleep .3
    y=$((y + 180))
    probe pointer "$((x + 30))" "$y"
    sleep .8
    scale=$(kad nativeCarryState | jq -r '.carryScale')
    # A pitch is a card and its gutter; the place the card left is a gap 0.7
    # of a pitch wide.
    pitch=$(jq -n --argjson w "$width" '$w * 1.1037')
    if jq -ne --argjson s "$scale" '$s > 0.325' >/dev/null; then
        # All of it shows: the row stands centred.
        x0=$(jq -n --argjson t "$t" --argjson h "$h" --argjson n "$items" --argjson s "$scale" --argjson p "$pitch" --argjson m "$mid" \
            '$m + (($t + (if $t >= $h then 0.7 else 0 end)) - ($n - 1 + 0.7) / 2) * $p * $s | floor')
    else
        # Lean the row along until the other is the next card past the held
        # card's middle, then rest; it stands within a pitch further on, where
        # a lean at the row's end, or a creep, brings it under the card.
        probe pointer "$(( t >= h ? mid + 600 : mid - 600 ))" "$y"
        for _ in {1..120}; do
            sleep .05
            read -r aim at < <(carry_aim)
            [[ $aim == gap ]] || continue
            if (( t >= h ? at >= t : at <= t + 1 )); then break; fi
        done
        probe pointer "$mid" "$y"
        sleep .6
        x0=$mid
    fi
    steps=(0 -40 40 -80 80 -120 120 -160 160)
    jq -ne --argjson s "$scale" '$s > 0.325' >/dev/null \
        || steps=($(for k in {0..14}; do echo $(( (t >= h ? 1 : -1) * k * 40 )); done))
    for dx in "${steps[@]}"; do
        probe pointer "$((x0 + dx))" "$y"
        sleep .5
        read -r aim at < <(carry_aim)
        [[ $aim == join && $at == "$t" ]] && break
    done
    sleep .4
    echo "state carry-$from $((x0 + dx)) $(kad nativeCarryState | jq -c '{lineCarrying, carryAim, carryIndex, carryScale}')"
    probe button false
    sleep .8
}
start_pass() {
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    trap 'kill "$client_pid" 2>/dev/null || true' EXIT
    sleep 1
    local width=$1 height=$2; shift 2
    for title in "$@"; do client titledCompanion "$title" "$width" "$height"; sleep .5; done
    sleep .5
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
        bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'map(select(.class == \"unload-client\" and .caption != \"unload-client\")) | length >= 2 and all(.onDisplay and (.hidden | not) and (.minimized | not))'"
    kill "$client_pid" 2>/dev/null || true
    wait "$client_pid" 2>/dev/null || true
    sleep .5
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 1280 720

# Pass one: windows small enough that the layout grows with every pick.
start_pass 500 400 'Card A' 'Card B' 'Card C' 'Card D'
a=$(idOf 'Card A') b=$(idOf 'Card B') c=$(idOf 'Card C') d=$(idOf 'Card D')
probe activateWindowId "$d" >/dev/null
sleep .8
report start
check 'by room: the monitor is sized Connected' bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce displaySizing Virtual-0) == connected ]]"
check 'by room: the context names it, and nothing was chosen' \
    context '[.displayContext.displays[] | select(.name == "Virtual-0" and .sizing == "connected" and (.sizingChosen | not))] | length == 1'
check 'start: the card in use is Active' context --arg id "$d" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
spread_key
report spread
check 'spread: Spread opens' presents cardLine
check 'spread: the selected card stands whole and centred' one_whole
# A wheel turned anywhere over Spread, open space too, moves the row a card.
probe pointer 1280 160
turn=-1
probe wheel "$turn"
sleep .8
if selected_is "$d" >/dev/null 2>&1; then turn=1; probe wheel "$turn"; sleep .8; fi
report wheel
check 'wheel: a turn in open space centres another card' selected_not "$d"
check 'wheel: it stands whole in the middle' one_whole
probe wheel "$((-turn))"
sleep .8
check 'wheel: turned back, the row comes back' selected_is "$d"
probe pointer 1280 720
# Search called over Spread is Spread's guest, here too: the row keeps its
# ordinary size beside it, and stands as it was once it has gone.
python3 - <<'PY' &
import dbus, json, time
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
assert json.loads(kadunce.beginLauncherGuest(bus.get_unique_name()))["accepted"]
time.sleep(60)
PY
owner_pid=$!
hosted() { kad workspaceContext | jq -e '.cardStage.launcherGuestActive' >/dev/null; }
for _ in {1..40}; do hosted && break; sleep .1; done
sleep .6
report search
check 'search: it opens as the guest of the monitor Spread' hosted
check 'search: Spread stays open' presents cardLine
check 'search: the cards beside it keep their ordinary size' \
    context '[.applications[] | select(.spreadRect) | .spreadRect.width] | length > 0 and all(. > 1000)'
# Only the guest may end itself; its program going away ends it too.
kill "$owner_pid" 2>/dev/null; wait "$owner_pid" 2>/dev/null
sleep .8
report search-gone
search_gone() { one_whole && context '.cardStage.presentation == "cardLine" and (.cardStage.launcherGuestActive | not)'; }
check 'search gone: Spread is open as it was' search_gone
pick "$a"
report paired
check 'pick: the layout shows' layout_shown
check 'pick: the picked card is a pane' is_pane "$a"
check 'pick: the card in use is the other pane' is_pane "$d"
check 'pick: two panes' panes 2
check 'pick: the picked pane has focus' focused "$a"
# Called over the layout, Search floats on its own: Spread stays closed.
check 'search over a layout: it floats, Spread stays closed' python3 - <<'PY'
import dbus, json
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
reply = json.loads(kadunce.beginLauncherGuest(bus.get_unique_name()))
assert not reply["accepted"] and reply.get("output"), reply
assert json.loads(kadunce.workspaceContext())["cardStage"]["presentation"] == "bento"
PY
spread_key
report spread-layout
check 'spread over a layout: Spread opens' presents cardLine
check 'spread over a layout: every pane is a card of its own' each_its_own
check 'spread over a layout: the selected card stands whole and centred' one_whole
pick "$b"
report grown
check 'grow: the layout shows' layout_shown
check 'grow: three panes' panes 3
check 'grow: the picked card is a pane' is_pane "$b"
check 'grow: the earlier panes stay panes' both_panes "$a" "$d"
before=$(rect "$d")
spread_key
pick "$d"
report pane-picked
check 'pane picked: the layout shows' layout_shown
check 'pane picked: it has focus' focused "$d"
check 'pane picked: nothing left the layout' panes 3
check 'pane picked: its pane stands where it stood' same_rect "$d" "$before"

# The choice is the person's, per display, and written down. The earlier name
# is accepted and saved under Solo's.
kad setDisplaySizing Virtual-0 active
sleep .6
report chosen-solo
check 'chosen: the monitor is sized Solo' context '[.displayContext.displays[] | select(.name == "Virtual-0" and .sizing == "solo" and .sizingChosen)] | length == 1'
check 'chosen: kaduncerc keeps it as solo' bash -c "kreadconfig6 --file kaduncerc --group Sizing --key Virtual-0 | grep -qx solo"
spread_key
report solo-spread
check 'solo sizing: every pane is still a card of its own in Spread' each_its_own
spread_key
kad setDisplaySizing Virtual-0 ''
sleep .6
check 'automatic again: sized by room' context '[.displayContext.displays[] | select(.name == "Virtual-0" and .sizing == "connected" and (.sizingChosen | not))] | length == 1'
check 'automatic again: kaduncerc no longer names it' bash -c "[[ -z \$(kreadconfig6 --file kaduncerc --group Sizing --key Virtual-0) ]]"
end_pass grow

# Pass two: windows so large that two panes fill the monitor.
start_pass 1100 900 'Card A' 'Card B' 'Card C'
a=$(idOf 'Card A') b=$(idOf 'Card B') c=$(idOf 'Card C')
probe activateWindowId "$b" >/dev/null
sleep .8
spread_key
pick "$a"
report full-pair
check 'full: two panes' panes 2
check 'full: the card in use and the pick are the panes' both_panes "$a" "$b"
slot_b=$(rect "$b")
spread_key
pick "$c"
report full-yield
check 'full: the layout shows' layout_shown
check 'full: still two panes' panes 2
check 'full: the pick is a pane' is_pane "$c"
check 'full: the pane used longest ago left and is a card' is_card "$b"
check 'full: the pane used last stayed' is_pane "$a"
check 'full: the pick took the slot that was given up' same_rect "$c" "$slot_b"

# A Stack fills one pane. The card that left is let go on a pane's card.
slot_c=$(rect "$c")
spread_key
select_card "$c"
carry_onto "$b" "$c"
report stacked
check 'stack: the card and the pane card are one Stack' \
    context --arg b "$b" --arg c "$c" '[.applications[] | select(.windowId == $b or .windowId == $c) | .stackId] | (length == 2 and (unique | length) == 1)'
open_selected
report stack-open
check 'stack: the layout shows' layout_shown
check 'stack: still two panes' panes 2
check 'stack: the other pane stayed' is_pane "$a"
face=$(kad workspaceContext | jq -r --arg b "$b" --arg c "$c" 'first(.applications[] | select((.windowId == $b or .windowId == $c) and (.hasCard | not))) | .windowId')
other=$([[ $face == "$b" ]] && echo "$c" || echo "$b")
check 'stack: one of the Stack is the pane, in that pane' same_rect "$face" "$slot_c"
check 'stack: the rest of it waits as a card' is_card "$other"
slot_a=$(rect "$a")
probe activateWindowId "$face" >/dev/null
sleep .5
probe key 15 125 >/dev/null
sleep 1.2
report stepped
check 'step: Meta+Tab puts the next card of the Stack in the pane' is_pane "$other"
check 'step: in the same place' same_rect "$other" "$slot_c"
check 'step: the card it replaced waits in the Stack' is_card "$face"
check 'step: the other pane did not move' same_rect "$a" "$slot_a"
check 'step: still two panes' panes 2
probe keyHeld 15 1 125 42 >/dev/null
sleep 1.2
report stepped-back
check 'step back: Meta+Shift+Tab returns the first card to the pane' is_pane "$face"
check 'step back: in the same place' same_rect "$face" "$slot_c"
face_title=$(probe windowFacts | jq -r --arg id "$face" 'first(.[] | select(.id == $id)) | .caption')
client closeCompanion "$face_title"
sleep 1.5
report closed-front
check 'close: the next card of the Stack takes the pane' is_pane "$other"
check 'close: in the same place' same_rect "$other" "$slot_c"
# A card waiting behind a pane is held aside, and one left hidden draws
# nothing, so it never takes the size its pane asks of it.
check 'close: shown, not left hidden' \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e --arg id '$other' 'first(.[] | select(.id == \$id)) | .hidden | not'"
check 'close: still two panes' panes 2
end_pass full
# Pass three: a side snap names a place, never a size. The middle half of an
# edge puts a card beside, a full-height column at that edge; its bottom
# quarter puts it under, at the bottom of the column there, and its top
# quarter over, at the top of it. Alone it leaves the rest open, and the next
# card fills the room left. This is the product owner's sequence: a terminal
# under on the right, a second under on the right, then a browser beside on
# the right.
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
for title in 'Term A' 'Term B' 'Browser' 'Notes' 'Mail'; do client titledCompanion "$title" 500 400; sleep .5; done
a=$(idOf 'Term A') b=$(idOf 'Term B') c=$(idOf 'Browser') n=$(idOf 'Notes') m=$(idOf 'Mail')
probe contactObserve >/dev/null
probe contactStart >/dev/null
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
probe activateWindowId "$a" >/dev/null
sleep .8
# Carries a window by a press inside it and lets it go at x y, keeping the
# place the carry previewed; held there, the wheel turned `notches` one at a
# time, slowly: toward the person for more than nothing, away for less.
snap() {
    local id=$1 title=$2 notches=${5:-0} x y i
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
    for (( i = 0; i < ${notches#-}; ++i )); do probe wheel $(( notches < 0 ? -1 : 1 )) >/dev/null; sleep .2; done
    kad nativeCarryState | jq -c '.destinationRect // empty' >"$XDG_RUNTIME_DIR/snap-preview"
    probe contactButton false >/dev/null
    sleep 1
}
previewed() { jq -e "$1" "$XDG_RUNTIME_DIR/snap-preview"; }
placed_at() { probe windowGeometry "$1" | jq -e --slurpfile p "$XDG_RUNTIME_DIR/snap-preview" '. == $p[0]'; }
geometry() { probe windowGeometry "$1" | jq -e "$2"; }
# The room a side snap left open offers the display's other cards.
offer() { kad nativeCarryState | jq -e "$@"; }
offers() { offer --arg id "$1" '(.roomOffer.cards // []) | index($id) != null'; }
# Presses the picture of a card the open room offers.
pick_offered() {
    local x y
    read -r x y < <(kad nativeCarryState | jq -r --arg id "$1" '.roomOffer as $o | ($o.cards | index($id)) as $i
        | $o.tiles[$i] | [.x + .width / 2, .y + .height / 2] | map(floor) | @tsv')
    probe pointer "$x" "$y" >/dev/null
    probe button true >/dev/null
    sleep .15
    probe button false >/dev/null
    sleep 1.2
}
check 'place: the first terminal is the Active card' context --arg id "$a" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
snap "$a" 'Term A' 2555 1200
report under
check 'under: the preview is the lower right corner' previewed '.x > 1250 and .y > 650 and .height < 760'
check 'under: it lands where the preview showed' placed_at "$a"
check 'under: the card stands alone as the layout' layout_shown
held_aside() { probe windowFacts | jq -e --arg b "$b" --arg c "$c" '[.[] | select(.id == $b or .id == $c) | .hidden] | length == 2 and all'; }
check 'under: the other cards are held aside, out of reach' held_aside
# The room above offers the display's other cards; Escape leaves it open.
check 'offer: the room above offers the other cards' offer --arg b "$b" --arg c "$c" '.roomOffer
    | .room.x > 1250 and .room.y < 200 and .room.height < 760
    and (.cards | index($b) != null and index($c) != null) and (.tiles | length) == (.cards | length)'
probe key 1 0 >/dev/null
sleep .6
check 'offer: Escape leaves the room open as it was' offer '.roomOffer.cards == null'
check 'offer: the snapped card stays where it was placed' placed_at "$a"
check 'offer: the other cards are still held aside' held_aside
spread_key
pick "$b"
report under-filled
check 'fill: the second terminal takes the room above' geometry "$b" '.x > 1250 and .y < 200 and .height < 760'
check 'fill: the first did not move' placed_at "$a"
snap "$b" 'Term B' 2555 1200
report under-again
check 'under again: the preview is the lower right corner' previewed '.x > 1250 and .y > 650 and .height < 760'
check 'under again: it lands where the preview showed' placed_at "$b"
check 'under again: the first terminal moved up in the same strip' geometry "$a" '.x > 1250 and .y < 200 and .height < 760'
check 'offer: the half left open offers the browser' offers "$c"
pick_offered "$c"
report browser-filled
check 'offer: a picture picked leaves the offer' offer '.roomOffer.cards == null'
check 'fill: the browser picked there takes the half left open' geometry "$c" '.x < 30 and .height > 1300'
check 'fill: the browser is in use' focused "$c"
snap "$c" 'Browser' 2555 720
report beside
check 'beside: the preview is the right column, full height' previewed '.x > 1250 and .height > 1300'
check 'beside: it lands where the preview showed' placed_at "$c"
check 'beside: the first terminal stays above, left of it' geometry "$a" '.x < 30 and .y < 200 and .height < 760'
check 'beside: the second terminal stays below, left of it' geometry "$b" '.x < 30 and .y > 650 and .height < 760'
before_a=$(rect "$a") before_b=$(rect "$b")
client closeCompanion 'Browser'
sleep 1.5
report beside-closed
check 'close: the first terminal keeps its place' same_rect "$a" "$before_a"
check 'close: the second terminal keeps its place' same_rect "$b" "$before_b"
check 'close: both still show' both_panes "$a" "$b"
check 'close: the layout still shows' layout_shown
# Past the open room, a card joins the column holding fewest, and side snaps
# still place among the columns.
spread_key
pick "$n"
spread_key
pick "$m"
report fourth
check 'fourth: the first pick tops the right column' geometry "$n" '.x > 1250 and .y < 200 and .height < 760'
check 'fourth: a card past the open room joins that column' geometry "$m" '.x > 1250 and .y > 650 and .height < 760'
check 'fourth: the terminals stay stacked on the left' geometry "$a" '.x < 30 and .y < 200 and .height < 760'
snap "$m" 'Mail' 5 720
report fourth-beside
check 'fourth beside: the preview is a full-height column at the left edge' previewed '.x < 30 and .height > 1300 and .width < 900'
check 'fourth beside: it lands where the preview showed' placed_at "$m"
check 'fourth beside: the terminals move inward, still stacked' geometry "$a" '.x > 800 and .x < 1000 and .y < 200 and .height < 760'
snap "$a" 'Term A' 2555 1200
report fourth-under
check 'fourth under: the preview is the bottom of the right column' previewed '.x > 1650 and .y > 650'
check 'fourth under: it lands where the preview showed' placed_at "$a"
check 'fourth under: the card above it moved up' geometry "$n" '.x > 1650 and .y < 200 and .height < 760'
snap "$n" 'Notes' 5 150
report fourth-over
check 'fourth over: the preview is the top of the left column' previewed '.x < 30 and .y < 200 and .height < 760'
check 'fourth over: it lands where the preview showed' placed_at "$n"
check 'fourth over: the card in that column moved down' geometry "$m" '.x < 30 and .y > 650 and .height < 760'
# Held at a side edge, the wheel sets the column's width like a knob: from
# its even quarter, three slow notches away from you pass a third's worth and
# catch at it, and the three columns beside it share the rest.
snap "$m" 'Mail' 5 720 -3
report knob-side
check 'knob side: the preview is a third of the display at the left edge' previewed '.x < 30 and .width > 790 and .width < 870'
check 'knob side: it lands where the preview showed' placed_at "$m"
check 'knob side: the columns beside it narrow to share the rest' geometry "$n" '.x > 820 and .x < 900 and .width > 520 and .width < 600'
# The card a side snap placed leaves for the top edge, seven slow notches
# narrowing it to three quarters, centred; Meta+S still opens Spread over the
# layout it left.
snap "$a" 'Term A' 1280 3 7
report snapped-left
check 'knob top: the preview is three quarters wide, centred' previewed '.width > 1800 and .width < 1960 and (.x + .width / 2 - 1280 | fabs) < 4'
check 'knob top: the Active card stands where the preview showed' placed_at "$a"
spread_key
report snapped-left-spread
check 'side gone: Meta+S opens Spread over the layout the snapped card left' presents cardLine
end_pass place

if (( failures > 0 )); then
    echo "FAIL: connected: $failures checks failed" >&2
    exit 1
fi
echo 'PASS: Connected plays picked cards into the layout, every pane a card of its own in Spread, a Stack fills one pane, a side snap places a card over, beside or under'
