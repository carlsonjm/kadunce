#!/usr/bin/env bash
# What Table needs from KWin, proved on KWin before Table is built: a desktop
# that is not current drawn on every display without switching to it, its
# windows kept drawing while it is shown, a switch the slide effect stays out
# of, and desktops created, renamed, filled and removed through KWin's own
# objects, each confirmed on KDE's own D-Bus. Kadunce is not loaded.
set -euo pipefail
trap 'echo "FAIL: table preview $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
tp() { qdbus6 org.kde.KWin /TableProof "$@"; }
client() { qdbus6 co.goodinput.TableClient /Client "$@"; }
vdm() { qdbus6 org.kde.KWin /VirtualDesktopManager "$@"; }
desktops() { qdbus6 --literal org.kde.KWin /VirtualDesktopManager org.kde.KWin.VirtualDesktopManager.desktops; }
current() { vdm org.kde.KWin.VirtualDesktopManager.current; }
# Where KDE itself says a window is, never where the effect says it put it.
kde_desktops() { qdbus6 org.kde.KWin /KWin org.kde.KWin.getWindowInfo "$(tp uuidOf "$1")" | sed -n 's/^desktops: //p'; }
share() { python3 "$(dirname "$0")/capture-band.py" "$1" 200 300 200 "$2"; }
at_least() { awk -v a="$1" -v b="$2" 'BEGIN { exit !(a >= b) }'; }
frames_in_a_second() {
    local before after
    client animate true
    sleep .2
    before=$(client counts); sleep 1; after=$(client counts)
    client animate false
    jq -n --argjson a "$before" --argjson b "$after" '$b | with_entries(.value -= $a[.key])'
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_table_proof; then break; fi
    sleep .1
done
test "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.isEffectLoaded slide)" = true
read -r x0 x1 <<<"$(tp outputs | jq -r '"\(.[0].x) \(.[1].x)"')"
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/table-proof-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
for attempt in {1..40}; do client counts >/dev/null 2>&1 && break; sleep .1; done

first=$(current)
second=$(tp create 1 Two)
desktops | grep -q "1, \"$second\", \"Two\""
test "$(vdm org.kde.KWin.VirtualDesktopManager.count)" = 2
tp rename "$second" Browser
desktops | grep -q "\"$second\", \"Browser\""
echo 'PASS: a desktop created and renamed through KWin shows on KDE D-Bus'

client open 'First left' c0392b 600 400
client open 'First right' 2980b9 600 400
client open 'Second left' 27ae60 600 400
client open 'Second right' 8e44ad 600 400
sleep 1.2
tp place 'First left' $((x0 + 100)) 100 600 400
tp place 'Second left' $((x0 + 100)) 100 600 400
tp place 'First right' $((x1 + 100)) 100 600 400
tp place 'Second right' $((x1 + 100)) 100 600 400
tp moveWindow 'Second left' "$second"
tp moveWindow 'Second right' "$second"
sleep .6
test "$(kde_desktops 'Second left')" = "$second"
test "$(kde_desktops 'Second right')" = "$second"
test "$(kde_desktops 'First left')" = "$first"
echo 'PASS: a window moved to a desktop is there on KDE D-Bus, and nothing else moved'

left=$((x0 + 250)) right=$((x1 + 250))
at_least "$(share "$left" c0392b)" 0.9
at_least "$(share "$right" 2980b9)" 0.9
hidden=$(frames_in_a_second)
test "$(jq '."Second left"' <<<"$hidden")" -le 1

tp resetWatch
tp preview "$second"
at_least "$(share "$left" 27ae60)" 0.9
at_least "$(share "$right" 8e44ad)" 0.9
at_least 0.02 "$(share "$left" c0392b)"
at_least 0.02 "$(share "$right" 2980b9)"
test "$(current)" = "$first"
echo 'PASS: the other desktop is drawn on every display without switching, with no blank first frame'
shown=$(frames_in_a_second)
test "$(jq '."Second left"' <<<"$shown")" -ge 10
test "$(jq '."Second right"' <<<"$shown")" -ge 10
test "$(jq '."First left"' <<<"$shown")" -ge 10
echo 'PASS: a previewed window draws live, and the hidden current desktop keeps drawing'

tp preview ''
sleep .4
test "$(current)" = "$first"
at_least "$(share "$left" c0392b)" 0.9
test "$(tp watched | jq -c .)" = '{"desktopChanges":0,"slideSeen":false}'
echo 'PASS: ending a preview changes nothing'

tp preview "$second"
sleep .3
tp commit
sleep .9
test "$(current)" = "$second"
test "$(tp watched | jq .slideSeen)" = false
test "$(tp holdsScreen)" = false
at_least "$(share "$left" 27ae60)" 0.9
echo 'PASS: committing a preview switches desktops without the slide'

tp resetWatch
qdbus6 org.kde.KWin /VirtualDesktopManager org.freedesktop.DBus.Properties.Set org.kde.KWin.VirtualDesktopManager current "$first"
sleep 1
test "$(current)" = "$first"
test "$(tp watched | jq .slideSeen)" = true
at_least "$(share "$left" c0392b)" 0.9
echo 'PASS: KDE switching still works and still slides'

third=$(tp create 2 'Proof client')
tp moveWindow 'First left' "$third"
sleep .3
test "$(kde_desktops 'First left')" = "$third"
tp moveWindow 'First left' "$first"
sleep .2
tp remove "$third"
sleep .3
test -z "$(desktops | grep -o "$third")"
test "$(vdm org.kde.KWin.VirtualDesktopManager.count)" = 2
test "$(kde_desktops 'First left')" = "$first"
echo 'PASS: a desktop made for one window, emptied and removed, is gone from KDE D-Bus'
