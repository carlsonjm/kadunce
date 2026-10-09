#!/usr/bin/env bash
# REQUESTS.md: placement requests on the display that owns cards. A request is
# placed only where it was last aimed; the middle makes a running application's
# window the Active card; a side edge pairs it into Bento; a launched
# application waits for its first window. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: placement: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
selected() { kad workspaceContext | jq -r 'first(.applications[] | select(.selected)) | .windowId'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" 2>/dev/null || true' EXIT
sleep .8
client colouredCompanion 'Working' 2e8b57 560 420
sleep .5
client colouredCompanion 'Other' 1e6fc8 600 440
sleep .8
probe contactObserve
probe contactStart >/dev/null
probe contactFocus >/dev/null
sleep .2
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
before=$(kad workspaceContext)
echo "before $(jq -c '{p: .cardStage.presentation, apps: [.applications[] | {windowId, title, appId, selected}]}' <<<"$before")"
working=$(jq -r 'first(.applications[] | select(.title == "Working")) | .windowId' <<<"$before")
other=$(jq -r 'first(.applications[] | select(.title == "Other")) | .windowId' <<<"$before")
app=$(jq -r 'first(.applications[] | select(.title == "Working")) | .appId' <<<"$before")
check "the protocol is version 1" test "$(kad placementProtocolVersion)" = 1

check "the dock's band aims nowhere" test "$(kad aimPlacement 640 790)" = none
check "the middle aims at the Active card" test "$(kad aimPlacement 640 400)" = card
check "the left edge aims at a Bento side" test "$(kad aimPlacement 5 300)" = left
check "a request placed away from its aim is refused" \
    test "$(kad placeApplication "$app" "$other" 640 400 refused)" = false
kad clearPlacementAim

# The middle: the running window becomes the Active card, as a tap does.
probe contactFocus >/dev/null
kad showActive
sleep .3
[[ $(selected) == "$other" ]] && target=$working || target=$other
check "aimed at the middle" test "$(kad aimPlacement 640 400)" = card
check "the running window is placed" test "$(kad placeApplication "$app" "{$target}" 640 400 middle)" = true
sleep .6
check "it is the Active card" context --arg id "$target" \
    '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .windowId == $id)'

# A launch: the request waits, and the first window of the application opens
# as the Active card.
check "aimed at the middle again" test "$(kad aimPlacement 640 400)" = card
check "a launch is accepted" test "$(kad placeApplication "$app" '' 640 400 launch)" = true
client colouredCompanion 'Launched' c88a1e 640 480
sleep 1.2
launched=$(kad workspaceContext | jq -r 'first(.applications[] | select(.title == "Launched")) | .windowId')
check "the launched window is the Active card" context --arg id "$launched" \
    '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .windowId == $id)'

# A side edge: the window placed there pairs into Bento with its neighbour.
check "aimed at the right edge" test "$(kad aimPlacement 1275 600)" = right
check "the side placement is accepted" test "$(kad placeApplication "$app" "$working" 1275 600 side)" = true
sleep 1.2
after=$(kad workspaceContext)
echo "after $(jq -c '{p: .cardStage.presentation, apps: [.applications[] | {windowId, title, selected}]}' <<<"$after")"
check "the display shows Bento" jq -e '.cardStage.presentation == "bento"' <<<"$after"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: placement: $failures checks failed" >&2; exit 1; fi
echo 'PASS: placement requests aim, refuse, activate, launch and pair as REQUESTS.md states'
