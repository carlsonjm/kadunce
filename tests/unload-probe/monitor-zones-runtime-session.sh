#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11: a display without cards takes the zones drawn for it
# in KDE's tile editor. Zones are drawn on the monitor (a left half, and a
# right half split top and bottom), and three windows there are organized:
# each takes one zone, a window too wide for a quarter takes the half, KWin
# holds each in its zone, a moved edge carries both panes with it and
# Kadunce leaves them there, switching to filling and back moves them out of
# their zones and in again, and switching off gives every window back out of
# its zone as it was. Needs the tablet fixture, so that a card display exists
# beside the monitor. Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
log="$(dirname "$XDG_RUNTIME_DIR")/session.log"
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: monitor zones: $name" >&2; failures=$((failures + 1)); fi
}
facts() { probe windowFacts | jq -c '[.[] | select(.normal) | {caption, output, minimized, x, y, width, height}]'; }
report() { echo "state $1 $(kad outputStageState 2>/dev/null | tr '\n' ' ')"; echo "facts $1 $(facts)"; }
# Runs a KWin script; what it prints with a tag lands in the session log.
script() {
    local name=$1 file="$XDG_RUNTIME_DIR/$1.js" id
    cat > "$file"
    id=$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadScript "$file" "$name")
    qdbus6 org.kde.KWin "/Scripting/Script$id" org.kde.kwin.Script.run
    sleep .5
    qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript "$name" >/dev/null
}
# Each zone's window rectangle and each window's zone, printed as tagged lines.
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
["unload-client", "Zone Two", "Zone Three", "Zone Four"].forEach(n => {
    const w = workspace.stackingOrder.find(w => w.caption == n);
    print("TILE$1 " + n.replace(" ", "_") + " " + (w && w.tile ? win(w.tile) : "none"));
});
JS
    sleep .3
}
zone_rects() { grep -a "ZONE$1 " "$log" | sed 's/.*ZONE[^ ]* //' | awk '{print $2, $3, $4, $5}'; }
tile_of() { grep -a "TILE$1 $2 " "$log" | tail -1 | sed "s/.*TILE[^ ]* $2 //"; }
# The window sits on a zone rectangle, to a pixel and a half.
on_zone() {
    local caption=$1 tag=$2 zones
    zones=$(zone_rects "$tag" | jq -R -s -c 'split("\n") | map(select(length > 0) | split(" ") | map(tonumber))')
    probe windowFacts | jq -e --arg c "$caption" --argjson zones "$zones" '
        first(.[] | select(.normal and .caption == $c)) as $w
        | any($zones[]; (($w.x - .[0]) | fabs) <= 1.5 and (($w.y - .[1]) | fabs) <= 1.5
            and (($w.width - .[2]) | fabs) <= 1.5 and (($w.height - .[3]) | fabs) <= 1.5)'
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .8
client titledCompanion 'Zone Two' 300 300
client titledCompanion 'Zone Three' 500 300
sleep .8
for caption in unload-client 'Zone Two' 'Zone Three'; do probe sendCaptionToOutput "$caption" Virtual-1; done
sleep .5
# Zones drawn as the person draws them in the tile editor: a left half, and
# the right half split top and bottom.
script draw <<'JS'
const out = workspace.screens.find(s => s.name == "Virtual-1");
const root = workspace.tilingForScreen(out).rootTile;
root.tiles[2].remove();
root.tiles[0].resizeByPixels(root.absoluteGeometry.width / 2 - root.tiles[0].absoluteGeometry.width, 4);
root.tiles[1].split(2);
JS
sleep .4
report before
read_zones 0
echo "zones: $(zone_rects 0 | tr '\n' ';')"
check "the layout drawn has three zones" test "$(zone_rects 0 | wc -l)" -eq 3

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
# A monitor fills itself until it is switched to its zones.
check "a monitor starts filling itself" test "$(kad usesDrawnZones)" = false
kad setUsesDrawnZones true
check "the switch takes the zones" test "$(kad usesDrawnZones)" = true
test "$(kad toggleBentoOnOutput Virtual-1)" = true
sleep 1.5
report organized
read_zones 1
for caption in unload-client 'Zone Two' 'Zone Three'; do
    check "organized: $caption sits in a zone" on_zone "$caption" 1
    check "organized: KWin holds $caption in a zone" bash -c "t='$(tile_of 1 "${caption// /_}")'; [[ -n \$t && \$t != none ]]"
done
check "organized: the three windows hold three different zones" \
    test "$(for c in unload-client Zone_Two Zone_Three; do tile_of 1 "$c"; done | sort -u | wc -l)" -eq 3
check "organized: the window too wide for a quarter has the half" \
    bash -c "$(declare -f probe); probe windowFacts | jq -e 'first(.[] | select(.caption == \"Zone Three\")) | .width >= 500'"

# The edge between the half and the right column moves; both panes follow
# KWin's zones, and Kadunce does not put them back.
script move <<'JS'
const out = workspace.screens.find(s => s.name == "Virtual-1");
const root = workspace.tilingForScreen(out).rootTile;
root.tiles[0].resizeByPixels(-160, 4);
JS
sleep 1.5
report edge-moved
read_zones 2
for caption in unload-client 'Zone Two' 'Zone Three'; do
    check "edge moved: $caption follows its zone" on_zone "$caption" 2
done

# A fourth window opens on the monitor with every zone held: it takes the
# smallest zone it fits, and that zone's window waits in the dock
# (CARD-LIFECYCLE.md §8).
probe pointer 1900 300
client titledCompanion 'Zone Four' 300 300
sleep 1.5
report arrival
read_zones 4
check "arrival: the new window sits in a zone" on_zone 'Zone Four' 4
check "arrival: one earlier window waits in the dock" \
    bash -c "$(declare -f probe facts); facts | jq -e '[.[] | select(.minimized)] | length == 1'"

# Switched back, the layout fills the monitor again and KWin holds no window
# in a zone; switched to zones once more, the windows take them again.
kad setUsesDrawnZones false
sleep 1.5
report filling
read_zones 5
for caption in unload-client Zone_Two Zone_Three; do
    check "filling: KWin no longer holds $caption in a zone" test "$(tile_of 5 "$caption")" = none
done
check "filling: the monitor still holds one layout" \
    bash -c "$(declare -f kad); kad outputStageState | grep -Eq '^Virtual-1\|.*\|[1-9]'"
kad setUsesDrawnZones true
sleep 1.5
report zones-again
read_zones 6
check "zones again: unload-client sits in a zone" on_zone unload-client 6

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep 1
report released
read_zones 3
for caption in unload-client Zone_Two Zone_Three Zone_Four; do
    check "released: KWin no longer holds $caption in a zone" test "$(tile_of 3 "$caption")" = none
done
check "released: no window is minimized" bash -c "$(declare -f probe facts); facts | jq -e 'all(.[]; .minimized | not)'"

if ((failures)); then echo "FAIL: monitor zones: $failures checks failed" >&2; exit 1; fi
echo 'PASS: the monitor takes the zones drawn for it, KWin owns their edges, and release leaves them'
