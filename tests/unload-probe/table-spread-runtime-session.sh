#!/usr/bin/env bash
# INPUT.md § Table, TABLE.md § Presentation: Table pulled down over Spread
# brings its tabs and leaves Spread showing, across every tab, until the pull
# reaches a workspace's cards; then that workspace previews at full size, and
# back up on the tabs Spread shows again. A menu bar flicked down over Spread
# leaves it showing until a tap chooses another tab. From the Active card a
# pull previews from the tabs, as before.
#
# Needs the tablet fixture and the tablet kit's direct edges. Every check is
# reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
current() { vdm org.kde.KWin.VirtualDesktopManager.current; }
table() { kad tableState | jq -r "$1"; }
at() { kad tableState | jq -r "$1 | floor"; }
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
presentation() { kad workspaceContext | jq -r '.cardStage.presentation'; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: table over spread: $name" >&2; failures=$((failures + 1)); fi
}
is() { test "$1" = "$2"; }
report() { echo "state $1 $(kad tableState | jq -c '{open, sticky, level, hovered, locked, preview}') presentation $(presentation)"; }
row() { at '.layout.tabs[0][1]'; }
cards_row() { kad tableState | jq -r '.layout.trayTop + 30 | floor'; }
pull() {
    probe down 90 "$1" 4
    for y in 14 28 44 56; do probe motion 90 "$1" "$y"; sleep .02; done
}
flick() {
    probe down 90 "$1" 4
    for y in 14 28 44; do probe motion 90 "$1" "$y"; sleep .02; done
    probe up 90
    sleep .5
}
slide() { probe motion 90 "$1" "$2"; sleep .12; }
lift() { probe up 90; sleep .8; }
tap() { probe down 91 "$1" "$2"; sleep .05; probe up 91; sleep .5; }
escape() { probe key 1 0; sleep .5; }
mkdir -p "$XDG_RUNTIME_DIR/z13-tablet-kit"
echo tablet >"$XDG_RUNTIME_DIR/z13-tablet-kit/posture"
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
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
kad showCardLine
sleep 1
check "Spread is open" is "$(presentation)" cardLine

# A pull over Spread: the tabs, then another tab, then its cards, then back.
pull 640
sleep .3
report pulled
check "pulled: Table is open on the tabs" is "$(table '.open') $(table '.level')" "true tabs"
check "pulled: Spread shows, not a preview" is "$(table '.preview')" ""
check "pulled: Spread stays open" is "$(presentation)" cardLine
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
report other-tab
check "another tab: it is under the finger" is "$(table '.hovered')" 1
check "another tab: Spread still shows" is "$(table '.preview')" ""
slide "$(at '.layout.tabs[1][0]')" "$(cards_row)"
report cards
check "its cards: the cards level" is "$(table '.level') $(table '.locked')" "cards 1"
check "its cards: that workspace previews" is "$(table '.preview')" "$two"
check "its cards: the desktop has not switched" is "$(current)" "$one"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
report back-on-tabs
check "back on the tabs: Spread shows again" is "$(table '.level') $(table '.preview')" "tabs "
slide "$(at '.layout.tabs[1][0]')" 20
check "at the edge: a lift would cancel" is "$(table '.cancelling')" true
lift
report cancelled
check "cancelled: Table closes and nothing changed" is "$(table '.open') $(current)" "false $one"
check "cancelled: Spread is still open" is "$(presentation)" cardLine

# A flick over Spread leaves a menu bar, with Spread showing until a tap.
flick 640
report flicked
check "flicked: a menu bar" is "$(table '.open') $(table '.sticky')" "true true"
check "flicked: Spread shows, not a preview" is "$(table '.preview')" ""
tap "$(at '.layout.tabs[1][0]')" "$(row)"
report tapped
check "a tapped tab previews its workspace" is "$(table '.preview')" "$two"
escape
check "Escape closes it and nothing changed" is "$(table '.open') $(current)" "false $one"

# Spread with Search in it, as the Tette Dot opens it, is Spread to Table.
hosted() { kad workspaceContext | jq -e '.. | objects | select(has("launcherGuestActive")) | .launcherGuestActive' >/dev/null; }
python3 - <<'PY' &
import dbus, json, time
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
assert json.loads(kadunce.beginLauncherGuest(bus.get_unique_name()))["accepted"]
time.sleep(60)
PY
owner_pid=$!
for attempt in {1..40}; do hosted && break; sleep .1; done
sleep .5
check "Search is in Spread" hosted
pull 640
sleep .3
report search-pulled
check "with Search: Table is open on the tabs" is "$(table '.open') $(table '.level')" "true tabs"
check "with Search: Spread shows, not a preview" is "$(table '.preview')" ""
check "with Search: Spread stays open" is "$(presentation)" cardLine
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
check "with Search: another tab, Spread still shows" is "$(table '.hovered') $(table '.preview')" "1 "
slide "$(at '.layout.tabs[1][0]')" "$(cards_row)"
report search-cards
check "with Search: its cards preview that workspace" is "$(table '.level') $(table '.preview')" "cards $two"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
check "with Search: back on the tabs, Spread shows again" is "$(table '.level') $(table '.preview')" "tabs "
slide "$(at '.layout.tabs[1][0]')" 20
lift
report search-cancelled
check "with Search: cancelled, nothing changed" is "$(table '.open') $(current) $(presentation)" "false $one cardLine"
check "with Search: Search is still in Spread" hosted
kill "$owner_pid" 2>/dev/null
wait "$owner_pid" 2>/dev/null
for attempt in {1..40}; do hosted || break; sleep .1; done

# From the Active card nothing changes: the tabs preview from the start.
probe activateWindowId "$(id_of 'One A')" >/dev/null
sleep .8
kad workspaceContext | jq -e '.cardStage.presentation == "active"' >/dev/null || { kad showCardLine; sleep 1; probe down 92 640 400; probe up 92; sleep 1; }
check "the Active card" is "$(presentation)" active
pull 640
sleep .3
report active-pulled
check "from the Active card: the tabs preview the workspace under the finger" test -n "$(table '.preview')"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
slide "$(at '.layout.tabs[1][0]')" "$(row)"
check "from the Active card: another tab previews" is "$(table '.preview')" "$two"
slide "$(at '.layout.tabs[1][0]')" 20
lift
check "from the Active card: cancelled" is "$(table '.open') $(current)" "false $one"

if ((failures)); then echo "FAIL: table over spread: $failures checks failed" >&2; exit 1; fi
echo 'PASS: Table over Spread leaves Spread showing on the tabs and previews from the cards; from the Active card it previews from the tabs'
