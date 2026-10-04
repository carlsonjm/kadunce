#!/usr/bin/env bash
# TETTEGOUCHE-CONTEXT.md § Companion guests: an application other than Search
# holds Spread's centre, answered on its own object.
#
# Over an Active card a companion is refused and nothing changes. In Spread it
# takes the centre; a second companion takes it from the first, which is told
# to close; a tap beside it closes it; and an application it launches replaces
# it, with the launch completed on the companion's own object. Needs the
# tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 studio.warbler.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: companion guest: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
hosted() { kad workspaceContext | jq -e '.. | objects | select(has("launcherGuestActive")) | .launcherGuestActive' >/dev/null; }
closed() { ! hosted; }
calls="$XDG_RUNTIME_DIR/companion-calls"
: >"$calls"
# A companion: it asks for the centre and records what Kadunce tells it.
companion() { # name, then optional launch identities
    python3 - "$1" "$calls" "${2:-}" <<'PY' &
import dbus, dbus.service, dbus.mainloop.glib, json, sys
from gi.repository import GLib
name, log, launch = sys.argv[1], sys.argv[2], sys.argv[3]
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus(private=True)
def note(line):
    with open(log, "a") as f: f.write(f"{name} {line}\n")
class Companion(dbus.service.Object):
    @dbus.service.method("org.example.Companion", in_signature="", out_signature="")
    def dismissGuest(self): note("dismissGuest")
    @dbus.service.method("org.example.Companion", in_signature="s", out_signature="")
    def completeGuestLaunch(self, token): note(f"completeGuestLaunch {token}")
Companion(bus, "/Companion")
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "studio.warbler.Kadunce")
assert kadunce.companionGuestProtocolVersion() == 1
reply = json.loads(kadunce.beginCompanionGuest(bus.get_unique_name(), "/Companion", "org.example.Companion"))
note(f"begin {json.dumps(reply, sort_keys=True)}")
if launch and reply.get("accepted"):
    note(f"prepare {bool(kadunce.prepareLauncherGuestLaunch(launch.split(','), name + '-token'))}")
loop = GLib.MainLoop(); GLib.timeout_add_seconds(30, loop.quit); loop.run()
PY
}
noted() { rg -q -- "$1" "$calls"; }
wait_noted() { for attempt in {1..40}; do noted "$1" && return 0; sleep .1; done; return 1; }

for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" $(jobs -p) 2>/dev/null || true' EXIT
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

# Over the Active card: refused, and Spread is not opened for it.
check "a card is Active" context '.cardStage.presentation == "active"'
companion over-active
wait_noted "over-active begin"
check "a companion over the Active card is refused" noted 'over-active begin .*"accepted": false'
check "the Active card stays Active" context '.cardStage.presentation == "active"'
check "no guest is held" closed

# In Spread: the first companion takes the centre.
kad showCardLine
sleep 1
companion first
wait_noted "first begin"
check "the first companion takes the centre" noted 'first begin .*"accepted": true.*"protocol": 1'
check "the centre is held" hosted
check "the reply carries the centre's geometry" noted 'first begin .*"card": \{'

# The second takes it from the first, which is told to close.
companion second
wait_noted "second begin"
check "the second companion takes the centre" noted 'second begin .*"accepted": true'
check "the first is told to close" wait_noted "first dismissGuest"
check "the second is not told to close" bash -c "! rg -q 'second dismissGuest' '$calls'"
check "the centre is still held" hosted

# A tap on empty space beside it closes it, on its own object.
width=$(kad workspaceContext | jq '.displayContext.displays[] | select(.role == "tablet") | .geometry.width | floor')
probe down 71 $((width / 2)) 20; probe up 71
check "a tap beside it tells the companion to close" wait_noted "second dismissGuest"
sleep .4
check "the centre is let go" closed

# An application it launches replaces it, completed on its own object.
apps=$(kad workspaceContext | jq -c '[.applications[] | .windowId]')
identity=$(kad workspaceContext | jq -r '[.applications[] | .appId // .entry // empty] | first // "unload-client"')
companion launcher "$identity,unload-client"
wait_noted "launcher prepare"
check "the launch is prepared" noted 'launcher prepare True'
client colouredCompanion 'Launched' 3a5fcd 560 420
check "the launch completes on the companion's object" wait_noted "launcher completeGuestLaunch launcher-token"
sleep 1
check "the launched application is the Active card" context --argjson before "$apps" \
    '.cardStage.presentation == "active" and (first(.applications[] | select(.selected)) | .title == "Launched")'
check "the centre is let go after the launch" closed

echo "calls: $(paste -sd'|' "$calls")"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: companion guest: $failures checks failed" >&2; exit 1; fi
echo "PASS: a companion is refused over the Active card; in Spread it holds the centre, yields it to a second companion that tells it to close, closes on a tap beside it, and hands over to an application it launches"
