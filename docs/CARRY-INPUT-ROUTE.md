# Carry release ownership

`CarryInputRoute.h` owns physical-stream retirement, not a destination or native
window. After verified native cancellation, acquire the still-held owner. An
interrupted adoption acquires in drain-only mode. Recheck the contact ticket after
the synchronous native call; if it already ended there is nothing left to drain.
Rejected native takeover does not acquire input.

Only the owner's release produces Release, once. Cancellation retires active
intent immediately but retains swallowed contact/button identities until up.
Duplicate down or an additional contact in the same stream cancels intent and
drains all swallowed downs. Other device tokens/modalities pass through. Physical
stream cancellation/device disappearance clears the corresponding drain. A
normal view cancellation must NOT clear held identities prematurely.

Actions are returned after state mutation; callbacks cannot replay an active
release. The adapter must forward lifecycle/device loss, reject invalid motion,
translate pointer device tokens and seat touch IDs, and cancel the destination
transaction when required. Destruction alone is not a promise of safe live unload:
the complete Effect must coordinate client cancellation and filter teardown.

## Evidence and limits

Thirteen standard tests pass. New unit coverage includes pointer/touch ownership,
foreign input, duplicate IDs, extra contacts, cancellation, one-shot release,
interrupted admission, device removal and ID reuse.

The private compositor ContactProbe now routes real mouse/touch motion and release
through this model into NativeCarryHandoff. Active/Bento accepted moves end once;
rejected moves stay native; interrupted and explicitly canceled carries drain
without drop. Source records remain valid and outcomes are consumed once.
Latest passing log is in CURRENT_STATE.

The Qt test client already reports a completed press/touch event after starting
its native move, even in the rejected/native control. Therefore the delivery
assertion compares counters before and after physical release: there must be no
ADDITIONAL release. It does not claim the original press never reached the app.

Tests use the private early filter, no production priority is selected. There is
no preview in these tests: release returns to source, not a destination commit.
Mixed-device passthrough has unit coverage; full compositor mixed-device ownership,
device-loss integration and Effect teardown still need combined verification.
No Effect instantiation, rendering, installation or repair promotion.
