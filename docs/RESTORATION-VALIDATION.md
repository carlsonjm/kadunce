# Restoration candidate — 2026-09-11

Resumed after RESTORATION-FREEZE.md. Source-only; no install, push, or repair
promotion. Existing user work preserved. The original failure now passes with
the candidate while the plain native negative control still fails.

## Policy approved by user

Instant disable wins. If disable/unload interrupts a previously minimized window
while its geometry is restoring, leave it visible rather than delay the kill
switch or minimize stale geometry. Normal completion re-minimizes it. This does
not guarantee that an unresponsive client will ever present the requested size.

## Implementation

DesktopStageController restores native state once with final minimization withheld.
If actual geometry and state already match, minimize synchronously. Otherwise a
controller-owned RestoredMinimization QObject observes matching frame geometry,
maximize/fullscreen/tile state and requested geometry/maximize/fullscreen state,
then disconnects before its one minimize request. Geometry is never rewritten by
the observer. Queued observation lets native signal handling finish first.

The observer cancels on close, native move/resize, output change/destruction,
activation, changed requested target, or its owner's destruction. Controller
output-retirement notifications and newer same-output layouts cancel explicitly.
Completed entries are pruned during subsequent controller work. A 2-second
deadline cancels only; it is not a success criterion or a resize retry.

Effect teardown cancels existing observers before input teardown/restoration.
Observers created by teardown restoration are destroyed with the controller,
before plugin unload. No detached callback, external service, nested event loop,
or durable client-owned plugin object was introduced.

## Evidence

- Eleven CTest tests pass in `/tmp/kadunce-restoration-build`.
- Source, control-package and live-control checks pass; persistent kill switch
  is active and registered.
- `/tmp/kadunce-unload-test.PTXoTN/session.log`: controller minimized/maximized
  restoration, exact visible geometry, normal/canceled/interrupted takeover,
  explicit pending cancellation, output retirement, native interaction, owner
  destruction and fullscreen/minimized restoration pass. Cancellation checked
  beyond the watchdog duration; destruction checks preserve visible geometry.
- `/tmp/kadunce-bento-candidate.oE481T/session.log`: full candidate two-output
  transfer/restoration/unload regression passes.
- `/tmp/kadunce-unload-test.BWvWbl/session.log`: plain KWin negative control still
  restores 1260x770 instead of 1280x800, without candidate controller calls.

## Limits and next step

This observes size/state, not a specific Wayland configure acknowledgment serial.
Dedicated client-close, watchdog-expiry-with-unresponsive-client, fresh-layout
re-entry and exact full-effect unload-during-pending-restore scenarios need more
coverage; owner destruction and general full-effect unload were tested separately.
Hardware monitor tests and touchscreen acceptance remain parked. Existing missing
initializer build warnings remain unchanged. Not a full-system install candidate.
Next: physical input-owner discovery and native carry renderer/drop integration;
retain the targeted pending-unload scenario in the final safety gate.

Installed plugin SHA256 remains
`6b752de4f4d9cedbf5ae533716c93135d6cf7c1b003ba2751df27b067a961448`;
repair source remains
`27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9`.
