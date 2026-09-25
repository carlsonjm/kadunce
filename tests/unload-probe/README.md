# Private unload-delivery probe

Run `bash tests/verify-unload-isolated.sh` from the repository. This builds a
separate test plugin, creates private XDG directories and D-Bus/Wayland sockets,
and starts virtual KWin. It never installs or loads into the user's compositor.
There are deliberately no CMake install rules. Do not load this input injector
manually into a real desktop; it is not a production debug interface.

The probe compiles the actual WorkspaceInputRouter source. Its workspace target
is a test double with virtual tablet geometry and grab counters. A registered
test input device advertises touch/pointer capabilities. Events enter KWin's
input pipeline, and a real Qt Wayland client counts delivered input. Normal
delivery is a positive control; zero events alone do not establish success.

Cases: pending hold removal, lifted touch rollback, router recreation before
release, lifted mouse teardown, mixed forwarded/consumed touch contacts, client
pointer ownership, and fresh input afterward. Double-click events count as
presses because Qt classifies successive clicks that way. Failed expectations
exit nonzero; a PASS marker is also required by the driver.

Limits: router destruction is tested, not removal of this shared library while
it is executing an input callback. This does not instantiate CardStageController
or prove hardware touch, real Active restoration, Tette's actual client, or the
full effect's mid-contact disable behavior. Native candidate/Bento load, restore,
and unload are a separate nested smoke test. No timing/performance conclusions.
# Bento interruption session

Run `KADUNCE_PROBE_SESSION=bento-session.sh bash tests/verify-unload-isolated.sh`
from the repository root for real-client synchronous restoration during Bento
placement. The test-only host treats virtual outputs as external. It compiles
the current production controller; it is not a production plugin replacement.
Interpret results against `../../docs/ARCHITECTURE.md` § Transfer transaction and
§ Carry session, and `../../docs/TESTING.md`.

## Native takeover probe

Run `KADUNCE_PROBE_SESSION=snap-session.sh bash tests/verify-unload-isolated.sh`.
This needs jq in addition to the existing test dependencies. SnapProbe is a
test-only filter, not production Effect. Native positive controls demonstrate
actual snapping; pointer/touch adoption demonstrates suppression plus shared
CarrySession pose updates. Early Shift and fresh native behavior are checked.
The probe validates the takeover boundary only. It does not establish production
runtime readiness or authorize live-session testing.

# Keyboard overlay session

Run `KADUNCE_PROBE_SESSION=keyboard-runtime-session.sh KADUNCE_RUNTIME_BUILD=<tablet build> bash tests/verify-unload-isolated.sh`
with the virtual-tablet fixture build. The private compositor starts a real
input-method client, `shuffle-keyboard` unless `KADUNCE_TEST_INPUT_METHOD`
names another, and the client opens windows with a focused text field at their
bottom edge, at their top edge, and one that reports no cursor. It asserts that
the compositor does not lift a window while Kadunce is loaded, that the
Active card's bottom edge ends one gutter above the keys with its top edge,
width and place unchanged and its bottom field on top of the keys, that it
follows taller and shorter keys, that it returns exactly, that a visible or
unreported cursor makes the same room, that Spread and Bento geometry are
untouched, and that unloading gives the compositor its own lift back. The
height change is sent through the keyboard's own `plasmakeyboardrc`.

# Typing a search by touch

Run `KADUNCE_PROBE_SESSION=keyboard-search-runtime-session.sh KADUNCE_RUNTIME_BUILD=<tablet build> bash tests/verify-unload-isolated.sh`
with the virtual-tablet fixture build. The same real input-method client runs,
and the Tettegouche search launcher opens beside it, first on its own and then
hosted by Kadunce inside Spread; `KADUNCE_TEST_LAUNCHER` names a launcher build,
the installed `tettegouche` otherwise. Each half asks the compositor which
window a touch on a letter reaches, types two letters and requires the text
cursor to advance with the launcher still open, then requires a touch outside
the launcher and the keys to close it. The hosted half taps a letter outside
the area Kadunce keeps for the launcher, which it reads from a lease taken and
dropped before the launcher starts. A launcher on the overlay layer fails the
first half, because it stacks above the keys; a Kadunce that reads a touch on
the keys as leaving the launcher fails the second.

# Starting in cards

Run `KADUNCE_PROBE_SESSION=start-cards-runtime-session.sh KADUNCE_RUNTIME_BUILD=<tablet build> bash tests/verify-unload-isolated.sh`
with the virtual-tablet fixture build. Kadunce is switched on with nothing
open, and the session requires that the tablet holds no card, that the first
window to open becomes the Active card, and that after the last card closes the
next window to open does the same. `desktop-runtime` on the same fixture
requires that switching on with a window already open makes it the Active card,
and that carrying that card to a side edge pairs it with the Active card rather
than taking the first-entry path an ordinary display takes.

# First carry session

Run `KADUNCE_PROBE_SESSION=first-carry-runtime-session.sh KADUNCE_RUNTIME_BUILD=<build> bash tests/verify-unload-isolated.sh`
with the ordinary build. An ordinary window is picked up by pointer and by
touch right after Kadunce loads. A neighbouring window closes while the carry
waits for the edge, and a tooltip closes after the edge takes it. The edge
must still take the carry and the drop must commit. The move trace names the
check that refuses a pickup, as it names the step that refuses a drop.

# Dialog sessions

Run `KADUNCE_PROBE_SESSION=dialog-runtime-session.sh` and
`KADUNCE_PROBE_SESSION=dialog-waiting-runtime-session.sh`, each with
`KADUNCE_RUNTIME_BUILD=<tablet build> bash tests/verify-unload-isolated.sh`.
The client opens a save dialog, a confirmation and a plain dialog over its own
window. Over its own Active card or Bento pane each floats at its own size, is
never a card, and leaves no mark on the layout; it leaves for Spread with its
card and comes back with it. Opened while the person is in Spread or using
another card, it waits hidden and unfocused while its application is marked as
wanting attention, and picking that application the way the dock does brings it
forward with the dialog on top. Switching Kadunce off gives a waiting dialog
back.

# Virtual desktop sessions

Run `KADUNCE_PROBE_SESSION=desktop-switch-runtime-session.sh` and
`KADUNCE_PROBE_SESSION=desktop-switch-bento-runtime-session.sh`, each with
`KADUNCE_RUNTIME_BUILD=<tablet build> bash tests/verify-unload-isolated.sh`.
Until each virtual desktop has its own cards, cards and layouts live on the
desktop where ownership started. The sessions add a second desktop, switch to
it and open a window there, and require that the window stays a plain window
that is drawn and takes touch, that Spread does not open there, that a layout on
the first desktop never takes the window or pulls the person back, and that the
first desktop's cards or layout come back exactly as they were, with a Spread
left open closed into Active. They photograph the screens with
`capture-png.py`, and the harness lets these sessions take screenshots.

# Monitor unplugged and plugged back in

Run `KADUNCE_PROBE_SESSION=output-unplug-runtime-session.sh KADUNCE_RUNTIME_BUILD=<tablet build> bash tests/verify-unload-isolated.sh`
with the virtual-tablet fixture build. Two windows are left on the second
display, one maximized, and `kscreen-doctor` removes and re-adds that display.
After every change a window on the tablet must be a card and a card's window
must be on the tablet, as KWin reports it. The window used last on the monitor
must arrive as the Active card with the other arrival and the previous Active
card behind it, be drawn in Spread, and another arrival must become Active when
picked; replugging
must not take a card back to the monitor or release the stage by re-maximizing
the Active card. The session repeats the hand test's switch-off-and-on while
the display is out, then switches off and requires ordinary windows. Every
check is reported, so one failure does not hide the rest.
