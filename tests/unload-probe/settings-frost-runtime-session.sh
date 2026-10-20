#!/usr/bin/env bash
# ITASCA-VISUAL-LANGUAGE.md: while Shuffle Settings stands over the work on a
# display, everything under its panels is frosted with the placement box's
# glass, and the panels stand over it as they are. Settings is played by the
# client under the program name Settings has, as a clear layer surface with
# Settings' title; the keyboard sheet, the same program under its own title,
# frosts nothing. A striped card says whether what is drawn over it is
# blurred, and the dock's band whether the panel is left as it was.
#
# Needs the tablet fixture. Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
here=$(cd "$(dirname "$0")" && pwd)
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: settings frost: $name" >&2; failures=$((failures + 1)); fi
}
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
surface_pid=
trap 'kill "$client_pid" $surface_pid 2>/dev/null || true' EXIT
sleep 1
client desktopSurface
client dockSurface 56
client stripedCompanion 'Stripes' 900 600
sleep .8
probe minimizeWindow "$(id_of unload-client)" true >/dev/null
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
probe activateWindowId "$(id_of 'Stripes')" >/dev/null
sleep 1
echo "facts $(probe windowFacts | jq -c 'map(select(.caption == "Stripes" or .caption == "") | {caption, class, layer, x, y, width, height})')"

# The middle of the striped card, and the dock's band under the cards.
read -r sx sy < <(probe windowFacts | jq -r 'first(.[] | select(.caption == "Stripes")) | "\(.x + .width / 2 - 80 | floor) \(.y + .height / 2 - 40 | floor)"')
contrast() { python3 "$here/capture-contrast.py" "$sx" "$sy" 160 80; }
band() { python3 "$here/capture-band.py" 100 764 400 28 101010; }
# The program name is what KWin knows a Wayland client's windows by.
surface() {
    cp "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" "$XDG_RUNTIME_DIR/shuffle-settings"
    "$XDG_RUNTIME_DIR/shuffle-settings" --surface "$1" &
    surface_pid=$!
    sleep 1.2
}
gone() { kill "$surface_pid" 2>/dev/null; wait "$surface_pid" 2>/dev/null; surface_pid=; sleep .8; }

before=$(contrast)
echo "contrast before: $before, dock band: $(band)"
check "the card's stripes are drawn sharp" test "$before" -ge 200

surface 'Shuffle Settings'
frosted=$(contrast)
dock=$(band)
echo "contrast under Settings: $frosted, dock band: $dock"
check "Settings is a layer surface of its own program" \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'any(.[]; .caption == \"Shuffle Settings\" and .class == \"shuffle-settings\")'"
check "under Settings the card is frosted" test "$frosted" -le 40
check "the dock stands over the frost as it was" python3 -c "import sys; sys.exit(float('$dock') < 0.95)"
gone
after=$(contrast)
echo "contrast after Settings: $after"
check "with Settings gone the stripes are sharp again" test "$after" -ge 200

surface 'Keyboard shortcuts'
sheet=$(contrast)
echo "contrast under the keyboard sheet: $sheet"
check "the keyboard sheet frosts nothing" test "$sheet" -ge 200
gone

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
(( failures == 0 ))
