#!/usr/bin/env bash
# A note being typed over a window has the keys, and the window it is stuck to
# stays the one in use: the workspace context names that window, with notes
# true, so a bar reading it keeps the window's name. The real Gooseberry, in a
# home of its own, brings a note stuck to the Active card out on it, ready to
# type, as a tap on the note in Spread does.
#
# KADUNCE_TEST_COMPANION names the Gooseberry binary; the installed one
# otherwise. Needs the tablet fixture.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
notes() { qdbus6 io.github.carlsonjm.gooseberry /StuckNotes "io.github.carlsonjm.Gooseberry.StuckNotes.$1" "${@:2}"; }
companion_binary=${KADUNCE_TEST_COMPANION:-gooseberry}
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: notes focus: $name" >&2; failures=$((failures + 1)); fi
}
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
home="$XDG_RUNTIME_DIR/notes-home"
mkdir -p "$home"
# KWin lists the windows only to a program whose desktop file asks for them,
# as Gooseberry's installed one does.
companion_path=$(command -v "$companion_binary")
mkdir -p "$XDG_DATA_HOME/applications"
cat >"$XDG_DATA_HOME/applications/io.github.carlsonjm.Gooseberry.desktop" <<DESKTOP
[Desktop Entry]
Type=Application
Name=Gooseberry
Exec=$companion_path
X-KDE-Wayland-Interfaces=org_kde_plasma_window_management,org_kde_plasma_virtual_desktop_management
DESKTOP
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
trap 'kill "$client_pid" "${companion_pid:-}" 2>/dev/null || true' EXIT
sleep .8
client colouredCompanion 'Beside' c88a1e 560 420
client colouredCompanion 'Holds the note' 2e8b57 560 420
sleep .8
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1.2
# One note, stuck to the window by its title and application.
mkdir -p "$home/notes"
cat >"$home/notes/2026-01-01-000000-test.md" <<'NOTE'
---
gooseberry: 2
created: 2026-01-01T00:00:00
changed: 2026-01-01T00:00:00
colour: yellow
stuck: true
window: "Holds the note"
app: "unload-client"
place: 0.300 0.300
tucked: false
---
Stuck to the window
NOTE
GOOSEBERRY_FOLDER="$home/notes" HOME="$home" XDG_CONFIG_HOME="$home/config" XDG_DATA_HOME="$home/share" \
XDG_STATE_HOME="$home/state" XDG_CACHE_HOME="$home/cache" \
QT_LOGGING_RULES="*.debug=true;qt.*.debug=false" QT_QPA_PLATFORM=wayland "$companion_path" --background >"$XDG_RUNTIME_DIR/notes.log" 2>&1 &
companion_pid=$!
for attempt in {1..40}; do
    notes ProtocolVersion >/dev/null 2>&1 && break
    sleep .1
done
focus() { kad workspaceContext | jq -c '.focus'; }
echo "before the note: $(focus)"
host=$(kad workspaceContext | jq -r '.applications[] | select(.title == "Holds the note") | .windowId')
app=$(kad workspaceContext | jq -r '.applications[] | select(.title == "Holds the note") | .appId')
check "the window is in use before the note" jq -e '.title == "Holds the note" and (has("notes") | not)' <<<"$(focus)"
sleep 1
note=$(notes Windows 2>/dev/null | grep -o '2026-01-01-000000-test' | head -1)
check "Gooseberry holds the note" test -n "$note"
echo "write on the note: $(notes Write "${note:-2026-01-01-000000-test}" "{$host}" "Holds the note" "$app" 2>&1)"
sleep 1.5
echo "typing on the note: $(focus)"
check "the window the note is stuck to stays the one in use" jq -e --arg id "$host" '.windowId == $id and .title == "Holds the note" and .notes == true' <<<"$(focus)"
notes Hide >/dev/null
sleep 1
echo "after the notes go: $(focus)"
check "with the notes put away the window is in use as before" jq -e '.title == "Holds the note" and (has("notes") | not)' <<<"$(focus)"

test "$(probe releaseRuntime)" = true
cat "$XDG_RUNTIME_DIR/notes.log" | grep -v "^qt\." | tail -30
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect kwin4_effect_kadunce
if ((failures)); then echo "FAIL: notes focus: $failures checks failed" >&2; exit 1; fi
echo "PASS: a note typed over a window keeps that window the one in use"
