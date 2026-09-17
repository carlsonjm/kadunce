# Native snap interception experiment — 2026-09-11

## Result

Pre-snap ownership transfer is possible on installed KWin 6.7.5-1.1 without
changing global options. This is a private experiment, not production integration
or a finished carry/preview. No desktop plugin, configuration or repair changes.

`KADUNCE_PROBE_SESSION=snap-session.sh bash tests/verify-unload-isolated.sh`

Passing evidence: `/tmp/kadunce-unload-test.OS6pOz/session.log` and `build.log`.
Temporary evidence can disappear; rerun the checked-in harness when needed.

## What was tested

A private virtual KWin loads a non-installable test effect and real Qt Wayland
client. Native moves start through Workspace::performWindowOperation(MoveOp).
Pointer/touch events then enter KWin's input pipeline, not direct calls to its
move-step function. Client setup/configure settles before beginning each move.

For both devices: left, right, top, bottom-left corner and Shift custom tiling.
Native positive controls produce an outline and tile/maximize on release.
The test filter at Effects priority instead begins the existing CarrySession,
publishes adoption before synchronous cancellation, and cancels the native move
before KWin's next step. Subsequent input updates the shared free-2D pose.

Assertions: zero native steps/outline activations after adoption, exactly one
native finish, no tile/maximize, unchanged native geometry, exact contact-minus-
anchor pose on both axes, consumed owner release. Electric tiling/maximize options
stay enabled. Removing the experiment leaves subsequent ordinary native tiling
working. Twenty main cases plus early-Shift and fresh-native controls pass.

An intermediate run (`hIoIVY`) reproduced native preview when Shift arrived
before first motion. The final experiment acquires ownership on that keyboard
entry too; `OS6pOz` verifies no early outline. This is why a motion-only filter
is insufficient. Do not present the intermediate expected-failure reproduction
as final runtime acceptance.

## Exact limits and production implications

- This cancels KWin's interactive move; it does not retain native dragging with
  snapping selectively disabled. A renderer must consume the moving carry pose.
  The test intentionally does not issue live window geometry changes per event.
- Production Effect is NOT instantiated. Its current move-start handler releases
  Active, and move-finished handlers interpret completion as a native drop.
  Adoption must prepare source ownership before those paths and mark cancellation
  as takeover, not drop/restore. Copying this test filter into Effect is unsafe.
- The test supplies owner/contact explicitly; actual titlebar/CSD origin-device
  identification, foreign input, keyboard cancellation/draining and close/output
  loss still need integration coverage. No visual free-carry or first-layout
  preview/commit is rendered, and no physical dock/touch acceptance is claimed.
- No system border registrations changed. Existing panel tests and source checks
  pass; real bottom dock behavior has not been re-tested with a new runtime because
  no new runtime exists. Keep bottom-center and Dock/AppletPopup paths untouched.
- Existing eleven native tests, source checks, control package and read-only live
  safety checks pass. Installed plugin and repair hashes match CURRENT_STATE.

## Next bounded integration

### Guarded boundary follow-up

`native/src/NativeMoveTakeover.h` now replaces the probe's inline cancellation
mechanism. Passing rerun: `/tmp/kadunce-unload-test.RImJ7t/session.log`.
The source reservation is still test-supplied, not a production controller
reservation. The observed native snapshot is distinct from the authoritative
pre-Active/Bento restore record, referenced by the immutable source token.

All earlier pointer/touch/Shift controls pass. New cases verify: finish classified
as takeover during synchronous cancellation; explicit cancellation and source
invalidation inside that callback cannot resurrect carry; recursive adoption is
rejected; injected screen-removal notification invalidates source; foreign device
cannot move/release the carry; exact source token/revision survives a one-shot
outcome; rejected reservation leaves native movement/snapping untouched. Output
loss uses notification injection, not physical hot-unplug. Actual client-close
signal handling exists but has not been exercised by this new harness.

The full Effect still does not call this boundary. This is not installed native
takeover, visual acceptance, or a completed gesture owner resolver. Next gate:
real source reservation before Active's manual-change release or Bento's native
move handler, followed by physical owner discovery and renderer/drop integration.

The mechanism gate passes; the complete production handoff gate remains open.
Use a single owner covering motion, relevant keyboard input, release and cancel.
Connect the carry renderer and source transaction before suppressing native motion.
Then wire validated edge intent into prepared Bento admission, never a second
native snap or after-the-fact geometry undo. User clarified the lone-window case:
snapping into Bento on an otherwise empty monitor uses Active presentation and
retains Bento membership for future arrivals. A companion chooser remains an
unapproved idea. Follow-up clarification: with other ordinary windows on that
monitor, the edge snap activates Bento for all eligible display-owned windows,
like Ctrl+B. This needs prepared batch activation, not the current arrival-only
first-layout planner. Precise geometry and input/rendering integration remain.
