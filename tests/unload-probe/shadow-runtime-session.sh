#!/usr/bin/env bash
# The shadow a card casts (CardShadow): unset, none falls; set, what lies under
# a window's lower edge darkens while the window keeps its own colour, and a
# window moved away leaves no shadow behind it.
#
# Needs OpenGL compositing, which Kadunce draws shadows with, so it runs on
# the device gate and not on a hosted runner, whose private compositor has none.
#
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
failures=0
check() {
    local name=$1; shift
    local said
    if said=$("$@" 2>&1); then echo "ok: $name"; else echo "FAIL: shadow: $name: $said" >&2; failures=$((failures + 1)); fi
}
Backdrop=f0f0f0 Card=3a6ea5
# How much of an area is one colour, from 0 to 1.
share() { python3 "$here/capture-band.py" "$@"; }
# Photographs the whole first display, for a person reading a failure.
photo() { [[ -z ${KADUNCE_SHADOW_PHOTOS:-} ]] || python3 "$here/capture-png.py" 0 0 1280 800 "$KADUNCE_SHADOW_PHOTOS/$1.png"; }
# An area at least `least` or at most `most` one colour, within a few frames.
# An area at least or at most `$6` one colour, within a few frames; a
# photograph that cannot be read is never either.
within() {
    local test=$1 value; shift
    for _ in {1..20}; do
        value=$(share "${@:1:5}") || value=
        [[ $value =~ ^[0-9.]+$ ]] && awk -v s="$value" -v t="$6" "BEGIN { exit !(s $test t) }" && return
        sleep .1
    done
    echo "${*:1:5} is ${value:-unreadable}"
    false
}
atLeast() { within '>=' "$@"; }
atMost() { within '<=' "$@"; }
depth() {
    if [[ -n $1 ]]; then kwriteconfig6 --file kwinrc --group Effect-kadunce --key CardShadow "$1" --notify
    else kwriteconfig6 --file kwinrc --group Effect-kadunce --key CardShadow --delete --notify; fi
    sleep .4
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 1200 780
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client colouredCompanion "Backdrop" "$Backdrop" 1100 700
sleep .5
client colouredCompanion "Card" "$Card" 400 300
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
backdrop=$(probe windowIdByCaption "Backdrop") card=$(probe windowIdByCaption "Card")
probe placeWindow "$backdrop" 60 40 >/dev/null
probe placeWindow "$card" 300 200 >/dev/null
probe activateWindowId "$card" >/dev/null
sleep .6
frame() { probe windowGeometry "$card" | jq -r "$1 | floor"; }
echo "card: $(probe windowGeometry "$card")"
echo "backdrop: $(probe windowGeometry "$backdrop")"
# Bands a few pixels below the card's lower edge, inside its width, and its middle.
below() { echo "$(( $(frame .x) + 20 )) $(( $(frame '.y + .height') + 3 )) $(( $(frame .width) - 40 )) 6"; }
middle() { echo "$(( $(frame .x) + 100 )) $(( $(frame .y) + 100 )) $(( $(frame .width) - 200 )) $(( $(frame .height) - 200 ))"; }
first=$(below)
photo unset
check "unset, nothing darkens below a window" atLeast $first "$Backdrop" .98
depth 1
photo deepest
check "deepest, the band below a window darkens" atMost $first "$Backdrop" .02
check "the window itself keeps its colour" atLeast $(middle) "$Card" .98
check "far from it, nothing darkens" atLeast 80 740 200 20 "$Backdrop" .98
probe placeWindow "$card" 700 300 >/dev/null
sleep .4
photo moved
read -r fx fy fw fh <<<"$first"
check "moved, it leaves no shadow where it stood" atLeast "$fx" "$fy" 320 "$fh" "$Backdrop" .98
check "moved, its shadow follows it" atMost $(below) "$Backdrop" .02
depth 0.3
read -r bx by bw bh <<<"$(below)"
check "shallower, the shadow still falls just below the edge" atMost "$bx" "$by" "$bw" 3 "$Backdrop" .02
depth 0
photo none
check "at none, no shadow falls" atLeast $(below) "$Backdrop" .98

if ((failures)); then echo "FAIL: shadow: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a card casts the shadow set for it, only while one is set, and moves it with the card'
