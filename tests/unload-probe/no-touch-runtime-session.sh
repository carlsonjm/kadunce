#!/usr/bin/env bash
# One touchscreen holds cards, and a machine with none has no card display: two
# plain monitors get Bento and the desktop, never a card, Spread or an Active
# card, whether asked by gesture, by D-Bus or by a window opening.
set -Eeuo pipefail
trap 'echo "FAIL: no touchscreen $LINENO" >&2' ERR
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
client ordinaryCompanion
client crossCompanion
sleep .8
context() { kad workspaceContext | jq -e "$1" >/dev/null; }
report() { printf '%s context %s\n' "$1" "$(kad workspaceContext | jq -c '{cards: .cardStage, bento: .desktopStage.active, displays: [.displayContext.displays[] | {name, role, bentoActive}], apps: [.applications[] | {title, hasCard}]}')"; }
report opened
no_cards='.cardStage.active == false and .cardStage.presentation == "inactive"
    and ([.applications[] | select(.hasCard)] | length == 0)'
context '[.displayContext.displays[] | select(.role == "tablet")] | length == 0'
context '(.displayContext.displays | length == 2)
    and ([.applications[] | select(.title | endswith("probe"))] | length == 2)'
context "$no_cards"
echo 'PASS: two plain monitors have no card display, and opening windows makes no cards'
kad showCardLine
sleep .5
context "$no_cards"
kad showActive
sleep .5
context "$no_cards"
probe down 81 640 795
for y in 740 680 600 520; do probe motion 81 640 "$y"; sleep .03; done
probe up 81
sleep .6
context "$no_cards"
echo 'PASS: neither a bottom swipe nor a request opens Spread or an Active card'
test "$(kad toggleBentoOnOutput Virtual-0)" = true
sleep .5
context '.desktopStage.active and ([.displayContext.displays[] | select(.name == "Virtual-0" and .bentoActive)] | length == 1)'
context "$no_cards"
echo 'PASS: a plain monitor still composes Bento, and Bento still makes no cards'
