#!/usr/bin/env bash
# INPUT.md § Spread: with Search in the middle of Spread, a tap on the card
# beside it closes Search and opens that card, whichever side it stands on; a
# tap on empty space closes Search and leaves Spread open.
#
# The tablet holds three cards. Each pass opens Spread and hosts Search in it,
# which stands the card that was centred on one side and the card before it on
# the other, then taps the middle of one side card, by touch or by pointer.
#
# Needs the tablet fixture: only a display that can own cards presents Spread.
# Every check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: guest beside: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
hosted() { kad workspaceContext | jq -e '.. | objects | select(has("launcherGuestActive")) | .launcherGuestActive' >/dev/null; }
closed() { ! hosted; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, apps: [.applications[] | {windowId, title, entry, selected, spreadRect}]}')"
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
touch=60
for pass in left:touch right:touch left:pointer right:pointer empty:touch; do
    side=${pass%:*} kind=${pass#*:} touch=$((touch + 1)) name=${pass/:/-}
    # KWin opens a window on the display under the pointer.
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    client colouredCompanion 'Beside Two' 2e8b57 560 420
    client colouredCompanion 'Beside Three' c88a1e 560 420
    sleep .8
    probe contactObserve
    probe contactStart >/dev/null
    probe contactFocus >/dev/null
    sleep .2
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    kad showCardLine
    sleep 1
    centred=$(kad workspaceContext | jq -r 'first(.applications[] | select(.selected)) | .windowId')
    # Search holds its place in Spread for as long as its owner is on the bus.
    python3 - <<'PY' &
import dbus, json, time
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "studio.warbler.Kadunce")
assert json.loads(kadunce.beginLauncherGuest(bus.get_unique_name()))["accepted"]
time.sleep(60)
PY
    owner_pid=$!
    for attempt in {1..40}; do hosted && break; sleep .1; done
    sleep .5
    report "$name-hosted"
    check "$name: Search is in Spread" hosted
    context=$(kad workspaceContext)
    width=$(jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width | floor' <<<"$context")
    # The middle of each card's visible part, left and right of Search.
    cards=$(jq -c --argjson w "$width" '[.applications[] | select(.spreadRect) | {windowId,
        x: ((([.spreadRect.x, 0] | max) + ([.spreadRect.x + .spreadRect.width, $w] | min)) / 2 | floor),
        y: (.spreadRect.y + .spreadRect.height / 2 | floor)}] | sort_by(.x)' <<<"$context")
    echo "$name: centred before Search $centred; cards beside it $cards"
    check "$name: one card stands each side of Search" jq -e 'length == 2' <<<"$cards"
    if [[ $side == empty ]]; then
        x=$((width / 2)) y=20 expected=$centred
    else
        pick=$([[ $side == left ]] && echo 0 || echo 1)
        read -r expected x y < <(jq -r --argjson i "$pick" '.[$i] | "\(.windowId) \(.x) \(.y)"' <<<"$cards")
    fi
    if [[ $kind == pointer ]]; then
        probe pointer "$x" "$y"; probe contactButton true; probe contactButton false
    else
        probe down "$touch" "$x" "$y"; probe up "$touch"
    fi
    sleep 1.2
    report "$name-tapped"
    check "$name: Search closes" closed
    if [[ $side == empty ]]; then
        check "$name: Spread stays open on the card it was on" \
            context --arg id "$expected" '.cardStage.presentation == "cardLine"
                and (first(.applications[] | select(.selected)) | .windowId == $id)'
    else
        check "$name: the tapped card opens" \
            context --arg id "$expected" '.cardStage.presentation == "active"
                and (first(.applications[] | select(.selected)) | .windowId == $id)'
    fi

    kill "$owner_pid" 2>/dev/null
    wait "$owner_pid" 2>/dev/null
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    probe contactDrop
    kill "$client_pid"
    wait "$client_pid" 2>/dev/null
    sleep .3
done

if ((failures)); then echo "FAIL: guest beside: $failures checks failed" >&2; exit 1; fi
echo 'PASS: a tap beside Search in Spread opens the card tapped, on either side, by touch and by pointer'
