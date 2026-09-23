#!/usr/bin/env bash
# CARD-LIFECYCLE.md §4 on the tablet, for a dialog whose application is not in
# front, in Spread or behind another card: it waits hidden and unfocused, its
# application is marked as wanting attention, and picking that application
# brings it forward with the dialog on top. Switching Kadunce off gives every
# waiting dialog back. dialog-runtime covers a dialog over its own card.
set -euo pipefail
trap 'echo "FAIL: dialog $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep .5
owner=$(kad workspaceContext | jq -r '.applications[0].windowId')
facts() { probe windowFacts; }
dialog() { facts | jq -e --arg c "$1" "first(.[] | select(.caption == \$c)) | $2" >/dev/null; }
owner_is() { facts | jq -e --arg id "$owner" "first(.[] | select(.id == \$id)) | $1" >/dev/null; }
context() { kad workspaceContext | jq -e "$1" >/dev/null; }
no_dialog_card() { context '[.applications[] | select(.title | test("^(Save|Confirm|Dialog) probe$")) | select(.hasCard)] | length == 0'; }
presentation() { context ".cardStage.presentation == \"$1\""; }
front() { context "first(.applications[] | select(.windowId == \"$1\")) | .hasCard and .focused"; }
log() { printf '%s %s\n%s cards %s\n' "$1" "$(facts | jq -c 'map({caption, parent, hidden, active, attention, width, height})')" "$1" "$( (kad workspaceContext 2>/dev/null || echo null) | jq -c 'if . then {p: .cardStage.presentation, apps: [.applications[] | {title, hasCard, focused}]} else null end')"; }

# Opened while the person is in Spread: it waits, and Spread stays.
for kind in saveDialog:'Save probe' plainDialog:'Dialog probe'; do
    kad showCardLine
    sleep .5
    client "${kind%%:*}"
    sleep 1
    log "spread ${kind#*:}"
    presentation cardLine
    dialog "${kind#*:}" '.hidden and (.active | not)'
    owner_is '.attention'
    no_dialog_card
    # Picked from the dock or a waiting row, the application comes forward
    # with its dialog on top and the dialog takes the keys.
    probe activateWindowId "$owner"
    sleep 1
    log "spread-picked ${kind#*:}"
    presentation active
    dialog "${kind#*:}" '(.hidden | not) and .active'
    owner_is '.attention | not'
    client closeDialogs
    sleep 1
done
echo 'PASS: in Spread a dialog waits hidden, and picking its application brings both forward'

# Opened by an application behind the card in front: the card in front keeps
# the screen and the keys.
client ordinaryCompanion
sleep 1
neighbor=$(kad workspaceContext | jq -r '.applications[] | select(.title == "Ordinary neighbor probe") | .windowId')
kad activateApplicationWindow "$neighbor"
sleep .8
front "$neighbor"
for kind in saveDialog:'Save probe' confirmDialog:'Confirm probe'; do
    client "${kind%%:*}"
    sleep 1
    log "behind ${kind#*:}"
    presentation active
    front "$neighbor"
    dialog "${kind#*:}" '.hidden and (.active | not)'
    owner_is '.attention'
    no_dialog_card
    client closeDialogs
    sleep 1
    front "$neighbor"
    owner_is '.attention | not'
done
client saveDialog
sleep 1
probe activateWindowId "$owner"
sleep 1
log behind-picked
presentation active
front "$owner" || dialog 'Save probe' '.active'
dialog 'Save probe' '(.hidden | not) and .active'
client closeDialogs
sleep 1
client closeCompanion 'Ordinary neighbor probe'
sleep 1
echo 'PASS: a dialog from an application behind the card in front waits until that application is picked'

# Switching Kadunce off gives back a dialog that was waiting.
kad showCardLine
sleep .5
client plainDialog
sleep 1
dialog 'Dialog probe' '.hidden'
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
sleep 1
log released
dialog 'Dialog probe' '(.hidden | not)'
owner_is '.attention | not'
echo 'PASS: switching Kadunce off gives a waiting dialog back'
