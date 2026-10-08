#!/usr/bin/env bash
# Table by pointer and keyboard, on the monitor beside the tablet. Its key
# opens it on the display under the pointer as a menu bar and closes it again;
# the wheel steps through the workspaces and the pointer previews the one it is
# over, without switching; a click enters it without the slide. The arrows
# choose a workspace and one of its cards, Enter makes the card Active, and
# Escape closes Table without changing anything. A push into the monitor's
# top-left corner opens it there, where KDE puts Overview, while a push against
# the rest of the top edge does not; Overview has the corner back once Kadunce
# is off. A hanging card dragged lifts for the pointer to carry to another
# workspace.
set -Eeuo pipefail
trap 'echo "FAIL: table pointer $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
tp() { qdbus6 org.kde.KWin /TableProof "$@"; }
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
current() { vdm org.kde.KWin.VirtualDesktopManager.current; }
table() { kad tableState | jq -r "$1"; }
# A layout position on the display Table is on, in whole pixels.
at() { kad tableState | jq -r "($1) as \$p | [(\$p[0] + .origin[0]), (\$p[1] + .origin[1])] | map(floor) | join(\" \")"; }
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
desktops_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .desktops | join(",")'; }
fx() { qdbus6 org.kde.KWin /Effects "$@"; }
overview_active() { fx org.kde.kwin.Effects.activeEffects | grep -qx overview; }
# A bare "! overview_active" never fails under errexit; this does.
overview_idle() { ! overview_active; }
# KDE's screen edge: the pointer pushed on against it, pushed back a pixel each
# time, until its delay has passed.
push() {
    for attempt in {1..20}; do
        probe pointer "$1" "$2"
        sleep .05
        [[ $(table '.open') == true ]] && break
        overview_active && break
    done
    return 0
}
# evdev key codes.
Esc=1 W=17 Enter=28 Left=105 Right=106 Down=108 Meta=125
press() { probe key "$1" "${2:-0}"; sleep .4; }
shortcut() { press "$W" "$Meta"; }
point() { probe pointer $1; sleep .15; }
click() { probe button true; sleep .05; probe button false; sleep .5; }
# A slide still running from an earlier switch is not the one being watched.
watch_from_rest() {
    for attempt in {1..50}; do [[ $(tp slideActive) == false ]] && break; sleep .1; done
    tp resetWatch
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_table_proof >/dev/null
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client colouredCompanion 'One A' c0392b 500 360
client colouredCompanion 'One B' 2980b9 500 360
client colouredCompanion 'Two A' 27ae60 500 360
sleep .8
one=$(current)
vdm createDesktop 1 Two
sleep .3
two=$(qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops | grep -o '[0-9a-f-]\{36\}' | grep -v "$one" | head -1)
probe sendToDesktop 'Two A' "$two"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
probe activateWindowId "$(id_of 'One A')" >/dev/null
sleep .5
test "$(table '.open')" = false

# The monitor starts where the tablet ends.
point '1900 400'
shortcut
test "$(table '.open')" = true
test "$(table '.sticky')" = true
test "$(table '.keys')" = true
test "$(table '.origin | join(",")')" = 1280,0
test "$(table '.hovered')" = 0
probe wheel 1
sleep .3
test "$(table '.hovered')" = 1
test "$(table '.preview')" = "$two"
probe wheel -3
sleep .3
test "$(table '.hovered')" = 0
test "$(current)" = "$one"
# Table holds the pointer while open, so no client beneath sets its shape.
test "$(probe mouseIntercepted)" = true
test -z "$(probe pointerFocus)"
shortcut
test "$(table '.open')" = false
test "$(table '.preview')" = ""
test "$(current)" = "$one"
point '1901 401'
test "$(probe mouseIntercepted)" = false
echo "PASS: Table's key is Meta+W, opens it on the display under the pointer holding the pointer, the wheel steps through the workspaces, and the key closes it"

watch_from_rest
shortcut
point "$(at '.layout.tabs[1]')"
test "$(table '.hovered')" = 1
test "$(table '.preview')" = "$two"
test "$(current)" = "$one"
click
test "$(table '.open')" = false
test "$(current)" = "$two"
test "$(tp watched | jq .slideSeen)" = false
echo 'PASS: the pointer previews the workspace it is over without switching, and a click enters it without the slide'

point '1900 400'
shortcut
test "$(table '.hovered')" = 1
press "$Left"
test "$(table '.preview')" = "$one"
press "$Down"
test "$(table '.level')" = cards
test "$(table '.locked')" = 0
press "$Right"
test "$(table '.card')" = 1
chosen=$(table '.previewCard')
test -n "$chosen"
test "$(current)" = "$two"
press "$Enter"
test "$(table '.open')" = false
test "$(current)" = "$one"
kad workspaceContext | jq -e --arg id "$(id_of "$chosen")" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id' >/dev/null
echo 'PASS: the arrows choose a workspace and one of its cards, and Enter makes the card Active there'

before=$(kad workspaceContext | jq -r '.cardStage.selectedCardId')
shortcut
press "$Right"
press "$Down"
press "$Esc"
test "$(table '.open')" = false
test "$(current)" = "$one"
test "$(kad workspaceContext | jq -r '.cardStage.selectedCardId')" = "$before"
echo 'PASS: Escape closes Table and nothing changes'

# Reaching the top of a window reaches the top edge; that is not Table.
point '1900 300'
push 1900 0
test "$(table '.open')" = false
overview_idle
echo "PASS: a push against the top edge, away from its corner, opens nothing"

# Overview loaded after Kadunce takes its corner again, as it does whenever it
# is reconfigured; Table holds the corner through both.
fx org.kde.kwin.Effects.isEffectLoaded overview | grep -qx true || fx org.kde.kwin.Effects.loadEffect overview >/dev/null
fx org.kde.kwin.Effects.unloadEffect overview
fx org.kde.kwin.Effects.loadEffect overview >/dev/null
sleep .3
point '1900 300'
push 1280 0
test "$(table '.open')" = true
overview_idle
test "$(table '.origin | join(",")')" = 1280,0
test "$(current)" = "$one"
press "$Esc"
fx org.kde.kwin.Effects.reconfigureEffect overview
qdbus6 org.kde.KWin /KWin reconfigure
sleep .6
point '1900 300'
push 1280 0
test "$(table '.open')" = true
overview_idle
echo "PASS: a push into the monitor's top-left corner opens Table there, not Overview, however Overview was loaded or reconfigured"
press "$Esc"

# The tablet's corner is reached across its Active card's top-left.
kad workspaceContext | jq -e '.cardStage.presentation == "active"' >/dev/null
for p in '640 400' '320 200' '120 80' '40 26' '14 9' '6 4' '2 1'; do point "$p"; done
push 0 0
test "$(table '.open')" = true
test "$(table '.origin | join(",")')" = 0,0
echo "PASS: a push across the Active card into the tablet's top-left corner opens Table there"

# A hanging card dragged lifts; the pointer carries it to a tab.
point "$(at '.layout.tabs[0]')"
point "$(at '.layout.tray[0]')"
test "$(table '.level')" = cards
lifted=$(table '.previewCard')
probe button true
sleep .8
test "$(table '.carrying')" = false
point "$(at '.layout.tray[0]' | awk '{print $1 + 30, $2}')"
test "$(table '.carrying')" = true
point "$(at '.layout.tabs[1]' | awk '{print $1, $2 + 40}')"
point "$(at '.layout.tabs[1]')"
probe button false
sleep .8
test "$(desktops_of "$lifted")" = "$two"
test "$(table '.open')" = true
press "$Esc"
test "$(table '.open')" = false
echo 'PASS: a card pressed and held still stays put, and dragged it lifts and the pointer carries it to another workspace'
test -z "$(kad ownershipViolations)"
echo 'PASS: no window had two owners'

# Overview here has no activities to draw with, so KWin is asked who holds the
# corner rather than Overview opened.
test "$(probe topLeftCornerHolders)" = kwin4_effect_kadunce
fx org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep .5
test "$(probe topLeftCornerHolders)" = overview
echo "PASS: Table alone holds the top-left corner, and with Kadunce off it is Overview's again"
