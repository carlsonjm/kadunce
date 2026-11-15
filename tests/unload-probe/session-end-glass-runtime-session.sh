#!/usr/bin/env bash
# ITASCA-VISUAL-LANGUAGE.md, Session end: the power question answered, the
# session ends behind its glass. The frost stays over the display while the
# cards are given back; a window asking something clears it; ended without
# going, the glass dissolves and the cards come back; and once the session
# has nothing left to show, the glass darkens to black. KDE's question is
# played by the client under the name of KDE's program for it, the session's
# end by KWin's own session calls, as Plasma makes them, and Plasma's sign-out
# program and a service of the session by their names on the bus.
#
# Needs the tablet fixture. Every check is reported.
set -uo pipefail
[[ ${XDG_RUNTIME_DIR:-} == /tmp/kadunce-unload-*/runtime ]] || exit 1
here=$(cd "$(dirname "$0")" && pwd)
probe() { qdbus6 org.kde.KWin /UnloadProbe "$@"; }
client() { qdbus6 co.goodinput.UnloadClient /Client "$@"; }
kad() { qdbus6 org.kde.KWin /Kadunce "$@"; }
session() { qdbus6 org.kde.KWin /Session "org.kde.KWin.Session.$1" "${@:2}"; }
# KWin's session states as its bus numbers them.
saving=0 quitting=1 normal=2
failures=0
check() {
    local name=$1; shift
    if "$@" >/dev/null 2>&1; then echo "ok: $name"; else echo "FAIL: session end glass: $name" >&2; failures=$((failures + 1)); fi
}
id_of() { probe windowFacts | jq -r --arg c "$1" 'first(.[] | select(.caption == $c)) | .id'; }
for attempt in {1..40}; do
    if qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kadunce_unload_probe; then break; fi
    sleep .1
done
probe pointer 600 400
"${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" &
client_pid=$!
surface_pid=
trap 'kill "$client_pid" $surface_pid 2>/dev/null || true' EXIT
sleep 1
client desktopSurface
client dockSurface 56
client stripedCompanion 'Stripes' 900 600
client askingCompanion 'Asks' c03a8a
sleep .8
probe minimizeWindow "$(id_of unload-client)" true >/dev/null
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect kwin4_effect_kadunce
sleep 1
probe activateWindowId "$(id_of 'Stripes')" >/dev/null
sleep 1
log="$(dirname "$XDG_RUNTIME_DIR")/session.log"
in_cards() { kad workspaceContext | jq -e '.cardStage.presentation == "active"'; }
check "the windows are cards before the question" in_cards
said() { rg -c "^Kadunce .* $1" "$log" || echo 0; }
# Lines said since the session began ending the last time.
since_end() { awk '/^Kadunce .* keeps the glass up while the session ends/ { n = NR } END { print n + 0 }' "$log"; }
# Said since then; the session's log can trail what was asked by a moment,
# so a line is waited for.
said_since() { local lines; lines=$(tail -n +"$(since_end)" "$log"); rg -q "^Kadunce .* $1" <<<"$lines"; }
seen_since() { local tries; for (( tries = 0; tries < 60; tries++ )); do said_since "$1" && return 0; sleep .1; done; return 1; }
not_gone_on() { ! said_since 'the session goes on'; }
goes_on() { seen_since 'the session goes on: the glass dissolves'; }
glass_up_past() { local tries; for (( tries = 0; tries < 60; tries++ )); do (( $(said 'keeps the glass up while the session ends') > $1 )) && return 0; sleep .1; done; return 1; }
band() { python3 "$here/capture-band.py" 100 100 400 200 "$1" 2>/dev/null; }
question() {
    cp "${KADUNCE_UNLOAD_PROBE_BUILD}/bin/unload-client" "$XDG_RUNTIME_DIR/ksmserver-logout-greeter"
    "$XDG_RUNTIME_DIR/ksmserver-logout-greeter" --surface "$1" &
    surface_pid=$!
    sleep 1.2
}
# Answered: the question goes, and the session begins to end at once.
answer() { kill "$surface_pid" 2>/dev/null; wait "$surface_pid" 2>/dev/null; surface_pid=; session setState "$1"; }

# Under systemd Plasma never says quitting: its sign-out program comes to the
# bus, the session saves and steps back to normal for good, and the windows
# close seconds later. Normal for all that time is no cancel.
ends=$(said 'keeps the glass up while the session ends')
check "Plasma's sign-out program takes its name" client ownName org.kde.Shutdown
question 'Sign out'
answer $saving
session setState $normal
check "the glass stays up for the sign-out" glass_up_past "$ends"
sleep 2.6
check "normal while the sign-out program runs is no cancel" not_gone_on
# Cancelled: the program leaves and no service of the session stops.
client dropName org.kde.Shutdown >/dev/null
check "the program gone and nothing stopped, the session goes on" goes_on
sleep 1.2
check "the cards come back after it" in_cards

# Without systemd Plasma saves the session, steps back to normal, then quits.
ends=$(said 'keeps the glass up while the session ends')
question 'Shut down'
answer $saving
session setState $normal
session setState $quitting
check "the glass stays up while the session ends" glass_up_past "$ends"
check "the cards are given back under it" seen_since 'lets every window go'
sleep .8
# The question's own glass begins to dissolve the frame it goes, before the
# session says it is ending; from then on nothing dissolves.
check "saving and stepping back on to quit is no cancel" \
    bash -c "tail -n +$(since_end) '$log' | rg -q '^Kadunce .* the session goes on' && exit 1 || exit 0"
check "the glass does not dissolve once the session is ending" \
    bash -c "tail -n +$(since_end) '$log' | rg -q '^Kadunce .* dissolves the frost on' && exit 1 || exit 0"

# Plasma asks every window to close; one holding unsaved work asks first.
# Its answer waits for every window to close, which the asking one does not.
session closeWaylandWindows >/dev/null 2>&1 &
sleep 1
check "a window asking something clears the glass" \
    seen_since 'clears the glass for a window asking something'
check "the asking window is shown" \
    bash -c "qdbus6 org.kde.KWin /UnloadProbe windowFacts | jq -e 'any(.[]; .caption == \"Unsaved probe\")'"

# Cancelled: the session goes on, the glass dissolves, the cards come back.
session setState $normal
check "ended without going, the session goes on" goes_on
sleep 1.2
check "the cards come back" in_cards

# Ended for good: with nothing it can wait for, the glass darkens to black.
# The windows closed, a service of the session leaves the bus as Plasma stops
# it and the sign-out program leaves; that is no cancel, however long the
# compositor is then left running.
ends=$(said 'keeps the glass up while the session ends')
client ownName org.kde.Shutdown >/dev/null
client ownName org.kde.kded6 >/dev/null
question 'Restart'
answer $saving
check "the glass stays up again" glass_up_past "$ends"
session setState $normal
client dropName org.kde.kded6 >/dev/null
client dropName org.kde.Shutdown >/dev/null
sleep 1.5
check "the glass darkens to black" seen_since 'darkens the glass to black'
sleep 1.5
check "a session being stopped does not go on" not_gone_on
black=$(band 000000)
if [[ $black =~ ^[0-9.]+$ ]]; then
    echo "black share: $black"
    check "the display is black" python3 -c "import sys; sys.exit(float('$black') < 0.95)"
else
    echo "the screen cannot be read back here; black is not checked in pixels"
fi

(( failures == 0 )) || exit "$failures"
echo 'PASS: the session ends behind its glass, clears for a window asking something, comes back when cancelled, and goes black'
