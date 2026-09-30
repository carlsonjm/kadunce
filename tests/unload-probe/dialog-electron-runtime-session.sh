#!/usr/bin/env bash
# CARD-LIFECYCLE.md §4 for Electron's own native message box on Wayland, the
# kind Code - OSS asks with before it opens a web page. Over its own Active
# card it is on top where a touch reaches it, and it is never a card; going to
# another card and back leaves it there.
#
# Needs the tablet fixture: only a display that can own cards presents Active.
set -euo pipefail
trap 'echo "FAIL: dialog electron runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
facts() { probe windowFacts; }
log() { printf '%s %s\n' "$1" "$(facts | jq -c 'map(select(.normal or .dialog) | {caption, parent, modal, hidden, active, attention, x, y, width, height})')"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
sleep 1
env -u ELECTRON_RUN_AS_NODE electron42 --ozone-platform=wayland --no-sandbox --disable-gpu "$(dirname "$0")/electron-box" &
box_pid=$!
trap 'kill "$client_pid" "$box_pid" 2>/dev/null || true' EXIT
sleep 4
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
id_of() { facts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
parent=$(id_of 'Electron box probe')
other=$(id_of 'unload-client')
probe activateWindowId "$parent"
sleep 1
kad workspaceContext | jq -e '.cardStage.presentation == "active"'
facts | jq -e 'first(.[] | select(.caption == "Electron box probe")) | .active'

# The box names the probe, which is the card in front, is shown above it, and
# a touch at its centre reaches it. Only the card in front is drawn, and a box
# with it.
on_top() {
    local f x y
    kad workspaceContext | jq -e --arg id "$parent" '.cardStage.presentation == "active"
        and .cardStage.selectedCardId == $id'
    f=$(facts)
    jq -e 'first(.[] | select(.caption == "Code - OSS"))
        | .parent == "Electron box probe" and (.hidden | not)' <<<"$f"
    jq -e 'map(.caption) | index("Code - OSS") > index("Electron box probe")' <<<"$f"
    x=$(jq 'first(.[] | select(.caption == "Code - OSS")) | .x + .width / 2 | floor' <<<"$f")
    y=$(jq 'first(.[] | select(.caption == "Code - OSS")) | .y + .height / 2 | floor' <<<"$f")
    probe windowAt "$x" "$y" | jq -e '.target.caption == "Code - OSS"'
}
no_box_card() {
    kad workspaceContext | jq -e '[.applications[] | select(.title == "Code - OSS") | select(.hasCard)] | length == 0'
}

kill -USR1 "$box_pid"
sleep 1.5
log opened
no_box_card
on_top
echo 'PASS: an Electron box opens on top of its Active card'

probe activateWindowId "$other"
sleep 1
log away
probe activateWindowId "$parent"
sleep 1
log back
no_box_card
on_top
echo 'PASS: going to another card and back leaves the box on top'

kill -USR2 "$box_pid"
sleep 1

# The box opens while another card is in front: it waits with its window,
# and choosing the window once brings it forward with the box on top.
probe activateWindowId "$other"
sleep 1
kill -USR1 "$box_pid"
sleep 1.5
log opened-away
probe activateWindowId "$parent"
sleep 1
log chosen
no_box_card
on_top
echo 'PASS: a box that opened behind another card comes forward with its window'

kill -USR2 "$box_pid"
sleep .5
test "$(probe releaseRuntime)" = true
sleep .5
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
