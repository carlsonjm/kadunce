#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11, Solo: the Active card snapped to a side with no
# awake card to pair stays Active, and the half beside it offers the
# display's sleeping cards; one picked wakes and pairs with it.
#
# Needs the tablet fixture: only a display that can own cards holds cards.
set -euo pipefail
trap 'echo "FAIL: solo offer runtime $LINENO" >&2' ERR
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]]
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep 1
client titledCompanion "Sleeper" 500 400
sleep .8
sleeper=$(probe windowIdByCaption "Sleeper")
[[ -n $sleeper ]]
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
main=$(probe windowIdByCaption unload-client)
[[ -n $main ]]
probe activateWindowId "$main" >/dev/null
sleep .8
test "$(probe minimizeWindow "$sleeper" true)" = true
sleep .8
kad workspaceContext | jq -e --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
read -r x y <<<"$(probe titleBarOf "$main" | jq -r '[.x, .y] | map(floor) | @tsv')"
probe pointer "$x" "$y" >/dev/null
client armMove
probe contactButton true >/dev/null
sleep .9
probe contactMotion "$((x + 200))" "$y" >/dev/null
sleep .1
probe contactMotion 1279 300 >/dev/null
sleep .4
kad nativeCarryState | jq -e '.destinationPreview and .destinationRect.x > 600'
probe contactButton false >/dev/null
offered=
for attempt in {1..30}; do
    offered=$(kad nativeCarryState | jq -c '.roomOffer // empty')
    [[ -n $offered && $offered != '{}' ]] && jq -e '.cards' <<<"$offered" >/dev/null && break
    sleep .1
done
echo "offered: $offered"
jq -e --arg id "$sleeper" '(.cards | index($id)) != null and .room.x < 100' <<<"$offered"
kad workspaceContext | jq -e --arg id "$main" '.cardStage.presentation == "active" and .cardStage.selectedCardId == $id'
read -r px py <<<"$(jq -r --arg id "$sleeper" '(.cards | index($id)) as $i | .tiles[$i] | [.x + .width / 2, .y + .height / 2] | map(floor) | @tsv' <<<"$offered")"
probe pointer "$px" "$py" >/dev/null
probe button true >/dev/null
sleep .15
probe button false >/dev/null
sleep 1.5
test "$(probe windowMinimized "$sleeper")" = false
kad nativeCarryState | jq -e '.roomOffer.cards == null'
kad workspaceContext | jq -e --arg a "$main" --arg b "$sleeper" \
    '[.applications[] | select((.windowId == $a or .windowId == $b) and .pane)] | length == 2'
# The panes glide into place; each stands in its half once they settle.
for attempt in {1..30}; do
    probe windowGeometry "$sleeper" | jq -e '.x < 100 and .width < 700' >/dev/null \
        && probe windowGeometry "$main" | jq -e '.x > 600' >/dev/null && break
    sleep .1
done
echo "panes: $(probe windowGeometry "$sleeper" | jq -c .) $(probe windowGeometry "$main" | jq -c .)"
probe windowGeometry "$sleeper" | jq -e '.x < 100 and .width < 700'
probe windowGeometry "$main" | jq -e '.x > 600'
echo 'PASS: a side snap with no awake partner offers the sleeping card beside it, and the pick pairs them'
