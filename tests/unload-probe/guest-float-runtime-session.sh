#!/usr/bin/env bash
# CARD-LIFECYCLE.md §11 and TETTEGOUCHE-CONTEXT.md § Launcher guest protocol 3:
# Search called while Spread is not shown floats over the work. Spread then
# opened under it, by Meta+S or by a swipe from the bottom bezel, asks it back,
# and it takes the centre with a card standing each side, as Search opened in
# Spread does, rather than floating over the card in the middle. Search hidden
# before Spread opens is not called back.
#
# The tablet holds three cards and is sized Connected, then Solo: Search over
# the work floats on either. Search is played by a launcher of the scene's own: it asks to
# begin when it starts and again when told to open, and can say it has hidden.
# Needs the tablet fixture: its touchscreen makes the bezel Kadunce's. Every
# check is reported, so a failure does not hide the ones after it.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: guest float: $name" >&2; failures=$((failures + 1)); fi
}
context() { kad workspaceContext | jq -e "$@"; }
hosted() { kad workspaceContext | jq -e '.cardStage.launcherGuestActive' >/dev/null; }
closed() { ! hosted; }
report() {
    echo "state $1 $(kad workspaceContext | jq -c '{p: .cardStage.presentation, guest: .cardStage.launcherGuestActive,
        apps: [.applications[] | {title, entry, selected, spreadRect}]}')"
}
calls="$XDG_RUNTIME_DIR/float-calls"
: >"$calls"
# Search's part: begin as it shows, begin again when told to open, and, with
# "hide", say it has hidden before anything else happens.
launcher() { # name, then optional "hide"
    python3 - "$1" "$calls" "${2:-}" <<'PY' &
import dbus, dbus.service, dbus.mainloop.glib, json, sys
from gi.repository import GLib
name, log, then = sys.argv[1], sys.argv[2], sys.argv[3]
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus(private=True)
kadunce = dbus.Interface(bus.get_object("org.kde.KWin", "/Kadunce"), "co.goodinput.Kadunce")
def note(line):
    with open(log, "a") as f: f.write(f"{name} {line}\n")
def begin(label):
    reply = json.loads(kadunce.beginLauncherGuest(bus.get_unique_name()))
    note(f"{label} {json.dumps(reply, sort_keys=True)}")
    return False
class Launcher(dbus.service.Object):
    @dbus.service.method("io.github.carlsonjm.Tettegouche", in_signature="", out_signature="")
    def open(self):
        note("open")
        GLib.idle_add(begin, "again")
    @dbus.service.method("io.github.carlsonjm.Tettegouche", in_signature="", out_signature="")
    def dismissGuest(self): note("dismissGuest")
    @dbus.service.signal("io.github.carlsonjm.Tettegouche", signature="b")
    def shown(self, visible): pass
search = Launcher(bus, "/Launcher")
assert kadunce.launcherGuestProtocolVersion() == 3
begin("begin")
if then == "hide":
    def hide():
        search.shown(False)
        note("hidden")
        return False
    GLib.timeout_add(200, hide)
loop = GLib.MainLoop(); GLib.timeout_add_seconds(30, loop.quit); loop.run()
PY
}
noted() { rg -q -- "$1" "$calls"; }
wait_noted() { for attempt in {1..40}; do noted "$1" && return 0; sleep .1; done; return 1; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done

touch=80
for pass in connected:key connected:swipe connected:hidden solo:key solo:swipe; do
    sizing=${pass%:*} opener=${pass#*:} touch=$((touch + 1)) name=${pass/:/-}
    # KWin opens a window on the display under the pointer.
    probe pointer 600 400
    "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
    client_pid=$!
    sleep .8
    client colouredCompanion 'Float Two' 2e8b57 560 420
    client colouredCompanion 'Float Three' c88a1e 560 420
    sleep .8
    probe contactObserve
    probe contactStart >/dev/null
    probe contactFocus >/dev/null
    sleep .2
    # The sizing is chosen once and read as the effect loads, so every pass
    # starts the same way.
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep .6
    kad setDisplaySizing Virtual-0 "$sizing"
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
    sleep .3
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
    sleep 1
    report "$name-start"
    check "$name: the tablet is sized $sizing" bash -c "[[ \$(qdbus6 org.kde.KWin /Kadunce displaySizing Virtual-0) == $sizing ]]"
    check "$name: Spread is closed" context '.cardStage.presentation != "cardLine"'

    launcher "$name" "$([[ $opener == hidden ]] && echo hide)"
    owner_pid=$!
    wait_noted "$name begin"
    check "$name: Search floats over the work" noted "$name begin .*\"accepted\": false.*\"output\": \"Virtual-0\""
    check "$name: Spread stays closed for it" context '.cardStage.presentation != "cardLine"'
    check "$name: nothing is in Spread's centre" closed
    [[ $opener == hidden ]] && wait_noted "$name hidden"

    read -r left width height < <(kad workspaceContext | jq -r '.displayContext.displays[] | select(.name == "Virtual-0")
        | "\(.geometry.x | floor) \(.geometry.width | floor) \(.geometry.height | floor)"')
    if [[ $opener == swipe ]]; then
        # Pulled from the bottom bezel past halfway, as a person opens Spread.
        x=$((left + width / 2))
        probe down "$touch" "$x" $((height - 4))
        for y in $(seq $((height - 20)) -24 $((height / 3))); do probe motion "$touch" "$x" "$y"; sleep .05; done
        sleep .2
        probe up "$touch"
    else
        # Meta+S by evdev code.
        probe key 31 125 >/dev/null
    fi
    sleep 1.2
    report "$name-spread"
    check "$name: Spread opens" context '.cardStage.presentation == "cardLine"'

    if [[ $opener == hidden ]]; then
        sleep .8
        check "$name: Search hidden is not called back" bash -c "! rg -q '$name open' '$calls'"
        check "$name: nothing takes Spread's centre" closed
    else
        check "$name: Spread asks Search back" wait_noted "$name open"
        check "$name: Search asks again and is taken in" wait_noted "$name again .*\"accepted\": true"
        for attempt in {1..20}; do hosted && break; sleep .1; done
        sleep .5
        report "$name-joined"
        check "$name: Search holds Spread's centre" hosted
        check "$name: Search's place is the middle of the tablet" noted \
            "$name again .*\"output\": \"Virtual-0\""
        # The cards stand beside Search, one each side, and none under its
        # middle, where the card in the middle stood while Search floated.
        cards=$(kad workspaceContext | jq -c --argjson l "$left" --argjson w "$width" '[.applications[]
            | select(.spreadRect) | .spreadRect | select(.x + .width > $l and .x < $l + $w)]')
        echo "$name: cards on the tablet $cards"
        middle=$((left + width / 2))
        check "$name: no card stands under Search's middle" \
            jq -e --argjson m "$middle" 'length > 0 and all(.[]; .x + .width <= $m or .x >= $m)' <<<"$cards"
        check "$name: a card stands each side of Search" \
            jq -e --argjson m "$middle" 'any(.[]; .x + .width <= $m) and any(.[]; .x >= $m)' <<<"$cards"
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

echo "calls: $(paste -sd'|' "$calls")"
if ((failures)); then echo "FAIL: guest float: $failures checks failed" >&2; exit 1; fi
echo 'PASS: Search floating as Spread opens, by the key or by a swipe, takes the centre beside the cards; hidden, it is not called back'
