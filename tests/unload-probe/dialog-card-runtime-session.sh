#!/usr/bin/env bash
# CARD-LIFECYCLE.md §4 for a dialog whose card is drawn in Spread or carried
# away. In Spread a waiting dialog is drawn on its application's card, where it
# stands over that application and at the card's scale, while it stays hidden
# and unfocused. Carried onto the monitor, the application takes its dialog
# along, and the dialog stands over it there, wholly on the display.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: dialog card: $name" >&2; failures=$((failures + 1)); fi
}
facts() { probe windowFacts; }
fact() { facts | jq -e --arg c "$1" "first(.[] | select(.caption == \$c)) | $2"; }
log() { printf '%s %s\n' "$1" "$(facts | jq -c 'map(select(.normal or .dialog) | {caption, output, hidden, active, x, y, width, height})')"; }
tint=c03a8a
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
# KWin opens a window on the display under the pointer.
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
probe sendCaptionToOutput unload-client Virtual-0
probe contactObserve
probe contactStart >/dev/null
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .8
owner=$(kad workspaceContext | jq -r 'first(.applications[] | select(.hasCard)) | .windowId')

# Opened while the person is in Spread, the dialog waits and is drawn on its
# card, taking about the share of the card it takes of its application.
kad showCardLine
sleep .5
client tintedDialog
sleep 1
log spread
check 'in Spread the dialog waits hidden and unfocused' fact 'Tinted dialog probe' '.hidden and (.active | not)'
read -r cx cy cw ch < <(kad workspaceContext | jq -r --arg id "$owner" \
    'first(.applications[] | select(.windowId == $id)) | .spreadRect
     | "\(.x | ceil) \(.y | ceil) \(.width | floor) \(.height | floor)"')
share=$(facts | jq -r --arg id "$owner" \
    '(first(.[] | select(.caption == "Tinted dialog probe")) | .width * .height)
     / (first(.[] | select(.id == $id)) | .width * .height)')
drawn=$(python3 "$(dirname "$0")/capture-band.py" "$cx" "$cy" "$cw" "$ch" "$tint")
echo "spread card $cx $cy $cw $ch dialog share $share drawn $drawn"
check 'in Spread the dialog is drawn on its card at the card scale' \
    awk -v d="$drawn" -v s="$share" 'BEGIN { exit !(d > s * 0.6 && d < s * 1.4) }'
# The dialog stands centred over its application, so on the card too.
middle=$(python3 "$(dirname "$0")/capture-band.py" "$((cx + cw / 2 - 10))" "$((cy + ch / 2 - 10))" 20 20 "$tint")
echo "spread card middle $middle"
check 'in Spread the dialog stands where it stands over its application' \
    awk -v m="$middle" 'BEGIN { exit !(m > 0.95) }'

# Picked, the application comes forward with its dialog on top.
probe activateWindowId "$owner"
sleep 1
log picked
check 'picked, the dialog shows' fact 'Tinted dialog probe' '.hidden | not'

# Where the dialog's middle stands on its application, in hundredths of it.
place() {
    facts | jq -r --arg id "$owner" '
        (first(.[] | select(.id == $id))) as $o
        | (first(.[] | select(.caption == "Tinted dialog probe"))) as $d
        | "\((($d.x + $d.width / 2 - $o.x) / $o.width * 100) | round) \((($d.y + $d.height / 2 - $o.y) / $o.height * 100) | round)"'
}
# Takes the application's own window from x y and holds it at x y.
carry() {
    probe contactFocus
    probe pointer "$1" "$2"
    client armMove
    probe contactButton true
    sleep .1
    probe contactMotion "$3" "$4"
    sleep .3
}

# The dialog covers its application's middle and stands wholly on the display
# its application is on.
over() {
    facts | jq -e --arg id "$owner" --argjson left "$1" --argjson right "$2" '
        (first(.[] | select(.id == $id))) as $o
        | (first(.[] | select(.caption == "Tinted dialog probe"))) as $d
        | ($o.x + $o.width / 2) as $mx | ($o.y + $o.height / 2) as $my
        | $d.x <= $mx and $d.x + $d.width >= $mx and $d.y <= $my and $d.y + $d.height >= $my
          and $d.x >= $left and $d.x + $d.width <= $right'
}

# Carried to the lower half of the tablet's right edge, the application pairs
# with the card beside it in the smaller pane, narrower than its dialog. The
# dialog stays over it and on the tablet.
client ordinaryCompanion
sleep 1
probe activateWindowId "$owner"
sleep 1
before=$(place)
carry 120 400 1275 600
probe contactButton false
sleep 1.2
log paired
paired() { kad outputStageState | grep -q '^Virtual-0|.*|2$'; }
check 'the application pairs as a Bento pane' paired
check 'paired, the dialog shows' fact 'Tinted dialog probe' '.hidden | not'
check 'paired, the dialog stays over its pane and on the tablet' over 0 1280
# Shorter than its pane, the dialog keeps its height on it.
check 'paired, the dialog keeps its height on its application' test "${before#* }" = "$(place | cut -d' ' -f2)"

# Carried on onto the monitor, beside the dialog. While the application is in
# hand the dialog is not left standing on the tablet.
before=$(place)
read -r px < <(fact unload-client '.x + 20')
read -r dx dy dw dh < <(fact 'Tinted dialog probe' '"\(.x) \(.y) \(.width) \(.height)"' | tr -d '"')
# Below the dialog, which covers the pane's width.
carry "$px" 700 1920 700
left=$(python3 "$(dirname "$0")/capture-band.py" "$dx" "$dy" "$dw" "$dh" "$tint")
log in-hand
echo "in hand: the dialog's old place on the tablet shows $left of it"
# A photograph does not see the card in hand itself, nor so the dialog drawn
# with it; that it travels with the card is seen on the tablet.
check 'in hand, the dialog is not left on the tablet' awk -v l="$left" 'BEGIN { exit !(l < 0.05) }'
probe contactButton false
sleep 1.2
log carried
after=$(place)
echo "dialog place on its application before $before carried $after"
check 'the application reaches the monitor' fact unload-client '.output == "Virtual-1"'
check 'its dialog goes with it and shows' fact 'Tinted dialog probe' '.output == "Virtual-1" and (.hidden | not)'
# Wider than its application, the dialog's place on it is its middle.
check 'carried, the dialog stands on the middle of its application' test "${after% *}" = 50
check 'carried, the dialog keeps its height on its application' test "${before#* }" = "${after#* }"
check 'carried, the dialog stays over it and on the monitor' over 1280 2560
client closeDialogs

if ((failures)); then echo "FAIL: dialog card: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a waiting dialog is drawn on its card in Spread, and a carried card takes its dialog along'
