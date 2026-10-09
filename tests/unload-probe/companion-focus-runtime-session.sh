#!/usr/bin/env bash
# A companion opened in Spread keeps Spread. The real Gooseberry, in a home of
# its own, opens its quick-note card while Spread shows three cards, the way a
# tap on its dock icon or on Search's Notes pill does. The card must take
# Spread's centre and Spread must stay; KWin handing focus back to the card
# that last had it is not a request to open that card.
#
# KADUNCE_TEST_COMPANION names the Gooseberry binary; the installed one
# otherwise. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
companion_binary=${KADUNCE_TEST_COMPANION:-gooseberry}
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: companion focus: $name" >&2; failures=$((failures + 1)); fi
}
state() { kad workspaceContext | jq -c '{p: .cardStage.presentation, guest: ([.. | objects | select(has("launcherGuestActive")) | .launcherGuestActive] | first), selected: (first(.applications[] | select(.selected)) | .title)}'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
home="$XDG_RUNTIME_DIR/companion-home"
mkdir -p "$home"
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" "${companion_pid:-}" 2>/dev/null || true' EXIT
sleep .8
client colouredCompanion 'Beside Two' 2e8b57 560 420
client colouredCompanion 'Beside Three' c88a1e 560 420
sleep .8
probe contactObserve
probe contactStart >/dev/null
probe contactFocus >/dev/null
sleep .2
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
companion() {
    HOME="$home" XDG_CONFIG_HOME="$home/config" XDG_DATA_HOME="$home/share" \
    XDG_STATE_HOME="$home/state" XDG_CACHE_HOME="$home/cache" \
    QT_QPA_PLATFORM=wayland "$companion_binary" "$@"
}
companion --background >/dev/null 2>&1 &
companion_pid=$!
sleep 2
echo "before spread $(state)"
kad showCardLine
sleep 1
echo "in spread $(state)"
check "Spread is shown" bash -c "kad() { qdbus6 org.kde.KWin /Kadunce \"\$@\"; }; kad workspaceContext | jq -e '.cardStage.presentation == \"cardLine\"'"

# Every state Kadunce reports while the card opens and settles.
python3 - >"$XDG_RUNTIME_DIR/states" <<'PY' &
import dbus, json, time
bus = dbus.SessionBus(private=True)
k = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
end = time.monotonic() + 3
last = None
while time.monotonic() < end:
    c = json.loads(k.workspaceContext())
    guest = None
    stack = [c]
    while stack:
        o = stack.pop()
        if isinstance(o, dict):
            if "launcherGuestActive" in o: guest = o["launcherGuestActive"]
            stack.extend(o.values())
        elif isinstance(o, list): stack.extend(o)
    now = (c["cardStage"]["presentation"], guest)
    if now != last: print(f"{time.monotonic():.3f} {now[0]} guest={now[1]}", flush=True); last = now
    time.sleep(0.01)
PY
watcher=$!
companion --capture >/dev/null 2>&1 &
wait "$watcher"
echo "states while the card opened:"; cat "$XDG_RUNTIME_DIR/states"
echo "settled $(state)"
check "Spread stays while the card opens" bash -c "! rg -q ' active ' '$XDG_RUNTIME_DIR/states'"
check "the card holds Spread's centre" bash -c "kad() { qdbus6 org.kde.KWin /Kadunce \"\$@\"; }; kad workspaceContext | jq -e '.cardStage.presentation == \"cardLine\" and ([.. | objects | select(has(\"launcherGuestActive\")) | .launcherGuestActive] | first)'"

# Closing it hands KWin's focus back to the card that had it: that is not a
# request to open that card, so Spread stays.
watch_states() {
python3 - "$1" >"$XDG_RUNTIME_DIR/$2" <<'PY' &
import dbus, json, sys, time
bus = dbus.SessionBus(private=True)
k = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
end = time.monotonic() + float(sys.argv[1])
last = None
while time.monotonic() < end:
    c = json.loads(k.workspaceContext())
    sel = next((a.get("title") for a in c.get("applications", []) if a.get("selected")), None)
    now = (c["cardStage"]["presentation"], sel)
    if now != last: print(f"{time.monotonic():.3f} {now[0]} selected={now[1]}", flush=True); last = now
    time.sleep(0.01)
PY
}
watch_states 3 closing
watcher=$!
width=$(kad workspaceContext | jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width | floor')
sleep .2
probe down 71 $((width / 2)) 20; probe up 71
wait "$watcher"
echo "states while the card closed:"; cat "$XDG_RUNTIME_DIR/closing"
echo "after closing $(state)"
check "Spread stays when the card closes" bash -c "! rg -q '^[0-9.]+ active ' '$XDG_RUNTIME_DIR/closing'"

# Search's Notes pill closes Search and opens the card in the same moment: the
# focus Search hands back must leave Spread for the card to take.
watch_states 3 reopening
watcher=$!
sleep .1
companion --capture >/dev/null 2>&1 &
wait "$watcher"
echo "states while the card reopened:"; cat "$XDG_RUNTIME_DIR/reopening"
echo "reopened $(state)"
check "Spread stays as the card reopens" bash -c "! rg -q '^[0-9.]+ active ' '$XDG_RUNTIME_DIR/reopening'"
check "the reopened card holds Spread's centre" bash -c "kad() { qdbus6 org.kde.KWin /Kadunce \"\$@\"; }; kad workspaceContext | jq -e '.cardStage.presentation == \"cardLine\" and ([.. | objects | select(has(\"launcherGuestActive\")) | .launcherGuestActive] | first)'"

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: companion focus: $failures checks failed" >&2; exit 1; fi
echo "PASS: a companion opened in Spread takes the centre, Spread stays as it closes and the focus returns, and it takes the centre again when reopened"
