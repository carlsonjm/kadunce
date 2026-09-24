#!/usr/bin/env bash
# Typing a search by touch. KWin starts a real input-method client for this
# session and the search launcher opens on its own, as it does on an ordinary
# Plasma panel. A finger on the keys must reach the keys, the launcher must
# stay open while it does, and a touch outside both must still close it.
# KADUNCE_TEST_LAUNCHER names the launcher build; the installed one otherwise.
set -euo pipefail
trap 'echo "FAIL: keyboard search $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
vk() { qdbus6 org.kde.KWin /VirtualKeyboard org.kde.kwin.VirtualKeyboard."$@"; }
launcher=${KADUNCE_TEST_LAUNCHER:-tettegouche}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
for attempt in {1..40}; do
    [[ $(vk available 2>/dev/null) == true ]] && break
    sleep .1
done
test "$(vk available)" = true
QT_IM_MODULE=wayland "$launcher" --standalone &
launcher_pid=$!
trap 'kill "$launcher_pid" 2>/dev/null || true' EXIT
open() { probe windowAt 640 60 | jq -e '.stack | any(.class == "tettegouche")' >/dev/null; }
for attempt in {1..60}; do open && break; sleep .1; done
open
vk forceActivate
sleep 1.5
state=$(probe keyboardState)
printf 'keyboard %s\n' "$state"
jq -e '.visible and .panel.height > 0' <<<"$state"
# A letter on the left of the second row, beside the launcher's sheet, where a
# touch that reached the launcher would read as a touch away from it. The
# blank side edges are the pointer surface's, which asks the portal first.
x=$(jq '(.panel.x + .panel.width * 0.2) | floor' <<<"$state")
y=$(jq '(.panel.y + .panel.height * 0.45) | floor' <<<"$state")
at=$(probe windowAt "$x" "$y")
printf 'at-keys %s\n' "$at"
jq -e '.target.inputMethod' <<<"$at"
echo 'PASS: a finger on the keys reaches the keys, not the launcher'
type_letter() {
    probe down 61 "$x" "$y"
    probe up 61
    sleep 1
}
type_letter
first=$(probe keyboardState)
type_letter
second=$(probe keyboardState)
printf 'after-keys %s %s %s\n' "$first" "$second" "$(probe windowAt 640 60)"
open
jq -e --argjson f "$first" '.visible and .cursor.height > 0 and .cursor.x > $f.cursor.x' <<<"$second"
echo 'PASS: the launcher and the keys stay up, and each letter reaches the search'
# Above the keys and beside the sheet is genuinely outside.
probe down 62 40 60
probe up 62
sleep 1
printf 'after-outside %s\n' "$(probe windowAt 640 60)"
! open
echo 'PASS: a touch outside the launcher and the keys still closes the launcher'

# Kadunce hosts the same launcher inside Spread, where Kadunce itself reads a
# touch outside the launcher as leaving it. The keys are not outside.
kill "$launcher_pid" 2>/dev/null || true
wait "$launcher_pid" 2>/dev/null || true
lower() { probe hideKeyboard; sleep 1; }
lower
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$launcher_pid" "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
kad showCardLine
sleep .5
# The area Kadunce keeps for the launcher in Spread. A lease taken and dropped
# here reads it; the launcher then takes its own.
guest=$(python3 - <<'PY'
import dbus, json
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "studio.warbler.Kadunce")
reply = json.loads(kadunce.beginLauncherGuest(bus.get_unique_name()))
assert reply["accepted"]
kadunce.endLauncherGuest()
print(json.dumps(reply["card"]))
PY
)
printf 'hosted-guest %s\n' "$guest"
QT_IM_MODULE=wayland "$launcher" &
launcher_pid=$!
hosted() { kad workspaceContext | jq -e '.. | objects | select(has("launcherGuestActive")) | .launcherGuestActive' >/dev/null; }
for attempt in {1..60}; do hosted && break; sleep .1; done
hosted
# A launcher that has just taken the text focus can miss the first raise.
for attempt in 1 2 3; do
    kad raiseKeyboard
    sleep 1
    probe keyboardState | jq -e '.visible' >/dev/null && break
done
state=$(probe keyboardState)
printf 'hosted-keyboard %s\n' "$state"
jq -e '.visible' <<<"$state"
# A letter left of that area and right of the blank pointer edge: q or a on
# the tablet, where the launcher's card does not reach.
x=$(jq -n --argjson k "$state" --argjson g "$guest" '
    [($g.x - 30), ($k.panel.x + $k.panel.width * 0.16)] | min | floor')
y=$(jq '(.panel.y + .panel.height * 0.45) | floor' <<<"$state")
jq -en --argjson k "$state" --argjson g "$guest" --argjson x "$x" '
    $x < $g.x and $x > $k.panel.x + $k.panel.width * 0.1'
printf 'hosted-key %s %s\n' "$x" "$y"
type_letter
first=$(probe keyboardState)
type_letter
second=$(probe keyboardState)
printf 'hosted-after-keys %s %s\n' "$first" "$second"
hosted
jq -e --argjson f "$first" '.visible and .cursor.height > 0 and .cursor.x > $f.cursor.x' <<<"$second"
echo 'PASS: inside Spread, the keys type into the launcher without closing it'
lower
probe down 63 40 60
probe up 63
sleep 1
! hosted
echo 'PASS: inside Spread, a touch outside the launcher and the keys still closes it'
