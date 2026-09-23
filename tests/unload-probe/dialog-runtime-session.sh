#!/usr/bin/env bash
# CARD-LIFECYCLE.md §4 on the tablet: a dialog follows its application and is
# never a card or a pane. Over its own Active card or its own pane it floats at
# its own size, and it leaves for Spread with its card and comes back with it.
# dialog-waiting-runtime covers a dialog whose application is not in front.
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

# Over its own Active card: its own size, on top, focused, and not a card.
presentation active
for kind in saveDialog:'Save probe' confirmDialog:'Confirm probe' plainDialog:'Dialog probe'; do
    client "${kind%%:*}"
    sleep 1
    log "active ${kind#*:}"
    presentation active
    dialog "${kind#*:}" '(.hidden | not) and .active and .width < 1260 and .height < 780'
    no_dialog_card
    owner_is '.hasCard != false and (.attention | not)'
    client closeDialogs
    sleep 1
    presentation active
    front "$owner"
done
echo 'PASS: over its own Active card a dialog floats at its own size and is never a card'

# Leaving for Spread hides it with its card; coming back shows it again.
client saveDialog
sleep 1
kad showCardLine
sleep 1
log spread-away
presentation cardLine
dialog 'Save probe' '.hidden and (.active | not)'
kad showActive
sleep 1
log spread-back
presentation active
dialog 'Save probe' '(.hidden | not) and .active'
client closeDialogs
sleep 1
echo 'PASS: a dialog leaves with its card for Spread and comes back with it'

# Over its own Bento pane: the layout keeps its shape and the dialog floats.
client companion
sleep .8
kad showCardLine
sleep .4
kad showActive
sleep .4
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .8
kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
panes=$(facts | jq -c '[.[] | select(.class == "unload-client") | {id, x, width}] | sort_by(.id)')
for kind in saveDialog:'Save probe' confirmDialog:'Confirm probe' plainDialog:'Dialog probe'; do
    client "${kind%%:*}"
    sleep 1
    log "bento ${kind#*:}"
    kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
    dialog "${kind#*:}" '(.hidden | not) and .active'
    no_dialog_card
    client closeDialogs
    sleep 1
    kad outputStageState | rg '^Virtual-0\|tablet\|.*\|2$'
    test "$(facts | jq -c '[.[] | select(.class == "unload-client") | {id, x, width}] | sort_by(.id)')" = "$panes"
done
echo 'PASS: over its own Bento pane a dialog floats and the layout keeps its shape'

