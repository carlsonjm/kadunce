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
Interpret results against `../../docs/CARRY-SESSION-CONTRACT.md` and
`../../docs/INTEGRATION-RELEASE-GATE.md`.

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
the compositor does not lift a window while Kadunce is loaded, that a covered
cursor pans only the Active card's contents to one gutter above the keys while
a photograph shows the card's frame where it was, that taller keys roll the
contents further and shorter ones roll nothing back, that the card returns
exactly, that a visible
or unreported cursor moves nothing, that Spread and Bento geometry are
untouched, and that unloading gives the compositor its own lift back. The
height change is sent through the keyboard's own `plasmakeyboardrc`.
