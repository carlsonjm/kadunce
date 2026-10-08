#!/usr/bin/env bash
# CARD-LIFECYCLE.md §8, While an individual card is Active: an application
# launched while a card is Active becomes the Active card in place. Spread is
# never presented on the way, so a surface grown over the Active card can hand
# over to the arriving window without the row showing around it.
#
# The tablet holds one Active card. A second window opens; a watcher reads the
# presentation every frame until it settles. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: active launch: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
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
sleep .8
probe contactObserve
probe contactStart >/dev/null
probe contactFocus >/dev/null
sleep .2
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
before=$(kad workspaceContext)
echo "before $(jq -c '{p: .cardStage.presentation, apps: [.applications[] | {windowId, title, selected}]}' <<<"$before")"
check "a card is Active before the launch" jq -e '.cardStage.presentation == "active"' <<<"$before"
working=$(jq -r 'first(.applications[] | select(.selected)) | .windowId' <<<"$before")
active_rect=$(probe windowGeometry "$working")

# Every presentation Kadunce reports from the launch until it settles.
python3 - >"$XDG_RUNTIME_DIR/presentations" <<'PY' &
import dbus, json, time
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
end = time.monotonic() + 2.5
while time.monotonic() < end:
    print(json.loads(kadunce.workspaceContext())["cardStage"]["presentation"], flush=True)
    time.sleep(0.008)
PY
watcher=$!
sleep .2
client colouredCompanion 'Arrival' c88a1e 640 480
wait "$watcher"
seen=$(sort -u "$XDG_RUNTIME_DIR/presentations" | paste -sd, -)
echo "presentations seen: $seen ($(wc -l <"$XDG_RUNTIME_DIR/presentations") reads)"
check "Spread is never presented on the way" bash -c "! rg -qv '^active$' '$XDG_RUNTIME_DIR/presentations'"
after=$(kad workspaceContext)
echo "after $(jq -c '{p: .cardStage.presentation, apps: [.applications[] | {windowId, title, selected}]}' <<<"$after")"
arrival=$(jq -r 'first(.applications[] | select(.title == "Arrival")) | .windowId' <<<"$after")
check "the arrival is the Active card" jq -e --arg id "$arrival" \
    '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .windowId == $id)' <<<"$after"
arrival_rect=$(probe windowGeometry "$arrival")
echo "Active before $active_rect; arrival $arrival_rect"
check "the arrival takes the Active card's place" jq -n -e --argjson a "$active_rect" --argjson b "$arrival_rect" '$a == $b'

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: active launch: $failures checks failed" >&2; exit 1; fi
echo 'PASS: an application launched over the Active card becomes the Active card in place, with Spread never shown'
