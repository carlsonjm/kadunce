#!/usr/bin/env bash
# ITASCA-VISUAL-LANGUAGE.md: while Shuffle Settings stands over the work on a
# display, everything under its panels is frosted with the placement box's
# glass, and the panels stand over it as they are. Settings is played by the
# client under the program name Settings has, as a clear layer surface that
# asks Kadunce for frost behind it as Settings does; the keyboard sheet, the
# same program asking nothing, frosts nothing. Kadunce says in the session's
# log where it laid the glass. Where the screen can be read back, a striped
# card says whether what is drawn over it is blurred, and the dock's band
# whether the panel is left as it was.
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
contrast() { python3 "$here/capture-contrast.py" "$sx" "$sy" 160 80 2>/dev/null; }
log="$(dirname "$XDG_RUNTIME_DIR")/session.log"
frost_lines() { rg -c '^Kadunce .* frosts "Virtual-0" under Shuffle Settings' "$log" || echo 0; }
band() { python3 "$here/capture-band.py" 100 764 400 28 101010; }
# The program name is what KWin knows a Wayland client's windows by.
surface() {
    cp "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" "$XDG_RUNTIME_DIR/shuffle-settings"
    "$XDG_RUNTIME_DIR/shuffle-settings" --surface "$@" &
    surface_pid=$!
    sleep 1.2
}
gone() { kill "$surface_pid" 2>/dev/null; wait "$surface_pid" 2>/dev/null; surface_pid=; sleep .8; }

# A screen that cannot be read back leaves the pixels unsaid; the log
# still says where the glass went.
before=$(contrast)
if [[ $before =~ ^[0-9]+$ ]]; then pixels=1; else pixels=; echo "the screen cannot be read back here; pixels are not checked"; fi
echo "contrast before: ${before:-none}, dock band: $(band 2>/dev/null)"
[[ -n $pixels ]] && check "the card's stripes are drawn sharp" test "$before" -ge 200

# The Context Panel's tail, laid under the windows.
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" --fade &
fade_pid=$!
trap 'kill "$client_pid" $surface_pid $fade_pid 2>/dev/null || true' EXIT
sleep .8
said=$(frost_lines)
surface 'Shuffle Settings' --frost
echo "facts $(probe windowFacts | jq -c 'map(select(.class == "shuffle-settings") | {caption, class, layer, x, y, width, height})')"
check "Settings is a layer surface of its own program" \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'any(.[]; .class == \"shuffle-settings\")'"
echo "said: $(rg -o '^Kadunce .* frosts "Virtual-0" under Shuffle Settings.*' "$log" | tail -1)"
check "Kadunce lays the frost under the panel" \
    rg -q '^Kadunce .* frosts "Virtual-0" under Shuffle Settings, laid under a panel' "$log"
check "the panel's fade laid under the windows is drawn over the frost" \
    rg -q '^Kadunce .* frosts "Virtual-0" under Shuffle Settings, laid under a panel .* with 1 panel fades over it' "$log"
check "the frost is said once for Settings" test "$(frost_lines)" -eq $((said + 1))
if [[ -n $pixels ]]; then
    frosted=$(contrast); dock=$(band)
    echo "contrast under Settings: $frosted, dock band: $dock"
    check "under Settings the card is frosted" test "$frosted" -le 40
    check "the dock stands over the frost as it was" python3 -c "import sys; sys.exit(float('$dock') < 0.95)"
fi
gone
check "with Settings gone the frost dissolves rather than cutting out" \
    rg -q '^Kadunce .* dissolves the frost on "Virtual-0" over [1-9][0-9]* ms' "$log"
if [[ -n $pixels ]]; then
    after=$(contrast)
    echo "contrast after Settings: $after"
    check "with Settings gone the stripes are sharp again" test "$after" -ge 200
fi

said=$(frost_lines)
surface 'Keyboard shortcuts'
check "the keyboard sheet frosts nothing" test "$(frost_lines)" -eq "$said"
if [[ -n $pixels ]]; then
    sheet=$(contrast)
    echo "contrast under the keyboard sheet: $sheet"
    check "under the keyboard sheet the stripes are sharp" test "$sheet" -ge 200
fi
gone

# The question before a restart, shut down or sign out, played by the
# client under the name of KDE's program for it, asking nothing.
cp "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" "$XDG_RUNTIME_DIR/ksmserver-logout-greeter"
"$XDG_RUNTIME_DIR/ksmserver-logout-greeter" --surface 'Restart' &
surface_pid=$!
sleep 1.2
check "the power question frosts under the panel as Settings does" \
    rg -q '^Kadunce .* frosts "Virtual-0" under the power question, laid under a panel .* with 1 panel fades over it' "$log"
gone

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
(( failures == 0 )) || exit "$failures"
echo 'PASS: Settings frosts the work under the panels, the panels stand over it as they are, and the keyboard sheet frosts nothing'
