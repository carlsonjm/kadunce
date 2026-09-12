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
See docs/NATIVE-INTERRUPTION-VALIDATION.md for coverage and limits.

## Native snap experiment

Run `KADUNCE_PROBE_SESSION=snap-session.sh bash tests/verify-unload-isolated.sh`.
This needs jq in addition to the existing test dependencies. SnapProbe is a
test-only filter, not production Effect. Native positive controls demonstrate
actual snapping; pointer/touch adoption demonstrates suppression plus shared
CarrySession pose updates. Early Shift and fresh native behavior are checked.
See docs/NATIVE-SNAP-VALIDATION.md before interpreting this as runtime readiness.
