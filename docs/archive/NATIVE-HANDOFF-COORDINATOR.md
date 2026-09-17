# Guarded native handoff

September 11, 2026. `NativeCarryHandoff.h` connects prepared source ownership to
correlated input and NativeMoveTakeover. Used by the private compositor probe,
not Effect. No installation or production input/rendering change.

Native start precedes app-request correlation. `stage` retains a read-only source
record for one event-loop turn, without invoking legacy release. `identify` checks
the original window, physical-contact proof and source validity around native
cancellation. It retains the source on success; it does not remove membership,
restore geometry or choose a destination. The native cancellation finish is
marked by ownsNativeFinish and must not enter legacy drop handling.

Unsupported/rejected identification leaves one queued native-start fallback.
Fallback is retired before callback invocation, so it cannot run twice. Actual
native finish must flush a pending start before legacy finish handling. Cancel
retires queued work immediately and cancels carry; it does not invoke fallback.
Synchronous cancellation during native finish cannot revive the transaction.
Physical-contact validity is checked during adoption only; after adoption,
source validity remains independent of the eventual finger/button release.
Source callbacks must be read-only and lifetime-safe. The coordinator must not
be destroyed synchronously from inside one of its own calls.

## Evidence and limits

Passing log: `/tmp/kadunce-unload-test.ATTkMG/session.log`. Twelve standard tests
and source/control/live safety checks pass. Installed plugin and repair unchanged.

Private tests exercise real Active and Bento source controllers with actual Qt
mouse/touch move requests: accept, explicit owner-proof rejection, and cancel
during native finish. They assert source reservation validity, guarded finish,
retained source, one fallback, and one-shot outcome preserving source identity.
An unidentified MoveOp takes one fallback; canceling it before the queued decision
suppresses fallback. Native protocol/system-titlebar observation regressions remain.

The fallback is a counted sentinel, not the complete Effect legacy handler.
Cancel recovery leaves the real source controller intact; source restoration is
explicit test cleanup. No visual carry or destination transaction is exercised.
System-titlebar identification was proved separately; the combined coordinator
test currently uses app-requested moves. Do not enable this half-connected path.

Next: connect carry painting and owner release/cancel draining to this coordinator,
then integrate guarded start/finish routing in Effect as one candidate. Preserve
tablet clipping and ordinary desktop input. Unsupported starts remain native.
