#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Every display: switched to zones, a display sized
# Connected plays its cards into the zones drawn for it, not into a layout of
# its own. Virtual-1 is large, sized Connected and chosen for Spread, and keeps
# KWin's own three columns, which count as drawn. A card picked from Spread
# takes a zone beside the one in use; Meta+B ends the layout into cards and
# Meta+B again lays every card there into the three zones, one each, KWin
# holding each in its zone. One card alone, carried to the right edge, takes
# the zone at the right. Switched off, no window is held in a zone. Every
# check is reported, so a failure does not hide the rest.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
log="$(dirname "$XDG_RUNTIME_DIR")/session.log"
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: connected zones: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
idOf() { probe windowIdByCaption "$1"; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, sel: .cardStage.selectedCardId,
        apps: [.applications[] | {title, output, hasCard, pane, minimized}]}')"
    echo "stages $1 $(kad outputStageState | tr '\n' ' ')"
    echo "facts $1 $(probe windowFacts | jq -c '[.[] | select(.normal) | {caption, output, x, y, width, height}]')"
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
bento_key() { probe key 48 125 >/dev/null; sleep 1.8; }
title_at() { probe titleBarOf "$1" | jq -r 'select(.x != null) | [.x, .y] | map(floor) | @tsv'; }
# A person's mouse presses a window's title bar, KWin's own, and carries it
# through each point given in turn before letting go.
title_drag() {
    local id=$1 x y; shift
    read -r x y <<<"$(title_at "$id")"
    [[ -n ${y:-} ]] || { echo "no title bar on $id"; return 0; }
    probe pointer "$x" "$y" >/dev/null
    sleep .1
    probe contactButton true >/dev/null
    sleep .1
    probe contactMotion "$((x + 30))" "$((y + 40))" >/dev/null
    sleep .15
    while (( $# >= 2 )); do
        probe contactMotion "$1" "$2" >/dev/null
        shift 2
        sleep .12
    done
    sleep .4
    probe contactButton false >/dev/null
    sleep 1.6
}
large_panes() { kad workspaceContext | jq '[.applications[] | select(.pane and (.hasCard | not) and .output == "Virtual-1")] | length'; }
panes_are() { [[ $(large_panes) == "$1" ]]; }
# Runs a KWin script; what it prints with a tag lands in the session log.
script() {
    local name=$1 file="$XDG_RUNTIME_DIR/$1.js" id
    cat > "$file"
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$file" "$name")
    qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run
    sleep .5
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "$name" >/dev/null
}
# Each zone of Virtual-1 and the zone each window is held in, as tagged lines.
read_zones() {
    script "zones$1" <<JS
const out = workspace.screens.find(s => s.name == "Virtual-1");
const root = workspace.tilingForScreen(out).rootTile;
function leaves(t, out = []) {
    if (t.tiles.length == 0) { out.push(t); return out; }
    for (let i = 0; i < t.tiles.length; ++i) leaves(t.tiles[i], out);
    return out;
}
function win(t) {
    const r = t.absoluteGeometry, q = t.relativeGeometry, p = t.padding;
    const l = q.x > 0 ? p / 2 : p, tp = q.y > 0 ? p / 2 : p;
    const rt = q.x + q.width < 1 ? p / 2 : p, b = q.y + q.height < 1 ? p / 2 : p;
    return [r.x + l, r.y + tp, r.width - l - rt, r.height - tp - b].map(Math.round).join(" ");
}
leaves(root).forEach((t, i) => print("ZONE$1 " + i + " " + win(t)));
["Mail", "Wallet", "Browser"].forEach(n => {
    const w = workspace.stackingOrder.find(w => w.caption == n);
    print("TILE$1 " + n + " " + (w && w.tile ? win(w.tile) : "none"));
});
JS
    sleep .3
}
zone_rects() { grep -a "ZONE$1 " "$log" | sed 's/.*ZONE[^ ]* //' | awk '{print $2, $3, $4, $5}'; }
tile_of() { grep -a "TILE$1 $2 " "$log" | tail -1 | sed "s/.*TILE[^ ]* $2 //"; }
held() { local t; t=$(tile_of "$1" "$2"); [[ -n $t && $t != none ]]; }
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
for id in "$mail" "$wallet" "$browser"; do probe serverDecorate "$id" >/dev/null; done
sleep .4
probe activateWindowId "$mail" >/dev/null
sleep .5
read_zones 0
echo "zones: $(zone_rects 0 | tr '\n' ';')"
check "Virtual-1 keeps KWin's own three columns" test "$(zone_rects 0 | wc -l)" -eq 3
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.8
kad setUsesDrawnZones true
check "the switch takes the zones" test "$(kad usesDrawnZones)" = true
sleep .5
spread_key
pick "$wallet"
report picked
read_zones 1
check 'picked: the large display shows two panes' panes_are 2
check 'picked: KWin holds the picked card in a zone' held 1 Wallet

bento_key
report ended
read_zones 2
check 'end: Meta+B ends the layout' panes_are 0
check 'end: no card is held in a zone' \
    bash -c "! grep -a 'TILE2 ' '$log' | grep -vq ' none\$'"
bento_key
report zoned
read_zones 3
check 'zones: Meta+B lays every card there out' panes_are 3
for caption in Mail Wallet Browser; do
    check "zones: KWin holds $caption in a zone" held 3 "$caption"
done
check 'zones: the three cards hold three different zones' \
    test "$(for c in Mail Wallet Browser; do tile_of 3 "$c"; done | grep -v '^none$' | sort -u | wc -l)" -eq 3

# One card left awake, carried by its title bar to the right edge, takes the
# zone at the right.
probe minimizeWindow "$wallet" true >/dev/null
sleep .8
probe minimizeWindow "$browser" true >/dev/null
sleep 1.2
report alone
check 'alone: the last card awake is the Active card' \
    context --arg id "$mail" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
read -r gx gy gw <<<"$(kad workspaceContext | jq -r 'first(.displayContext.displays[] | select(.name == "Virtual-1")) | .geometry | [.x, .y, .width] | map(floor) | @tsv')"
title_drag "$mail" "$((gx + gw - 300))" "$((gy + 500))" "$((gx + gw - 60))" "$((gy + 500))" "$((gx + gw - 3))" "$((gy + 500))"
report snapped
read_zones 6
right_zone=$(zone_rects 6 | sort -n -k1 | tail -1)
echo "right zone: $right_zone, Mail: $(tile_of 6 Mail)"
check 'snapped right: the card is a pane' panes_are 1
check 'snapped right: KWin holds it in the zone at the right' test "$(tile_of 6 Mail)" = "$right_zone"

kad setUsesDrawnZones false
sleep 1.5
report filling
read_zones 4
for caption in Mail Wallet Browser; do
    check "filling: KWin no longer holds $caption in a zone" test "$(tile_of 4 "$caption")" = none
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep 1
read_zones 5
for caption in Mail Wallet Browser; do
    check "released: KWin no longer holds $caption in a zone" test "$(tile_of 5 "$caption")" = none
done
if ((failures)); then echo "FAIL: connected zones: $failures checks failed" >&2; exit 1; fi
echo 'PASS: switched to zones, Meta+B on a Connected display fills its zones with cards, KWin holding each'
