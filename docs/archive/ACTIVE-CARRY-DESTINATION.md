# Active source departure

CardStageController::transferNativeCarryToDesktop connects a PreparedCarrySource
to the existing synchronous desktop receiver. It validates source ownership,
native-move completion, destination identity/geometry and prepares model removal.
The receiver solves admission before invoking the source commit. A rejection
does not alter membership, Active presentation or its original restore record.

Commit revalidates the reservation and output, then removes the card once and
retires its Active restore record without applying that old geometry. Remaining
tablet cards stay organized in Card Line, matching active-card removal behavior;
an empty tablet stage becomes inactive. Receiver publication precedes visual
cleanup, and native placement follows publication. A later tablet release must
not restore the departed window. Duplicate source tokens reject.

The existing receiver still normalizes the destination's desktop restore state
to the requested arrival rectangle. This change does not transplant source
fullscreen/maximize flags to the destination or enable edge-triggered activation.

## Evidence and remaining work

Private real-controller checks cover receiver rejection, ordinary desktop arrival,
and existing Bento arrival with destination membership visible before cleanup.
They also check one commit/cleanup, duplicate rejection, and tablet release not
pulling the arrival back. Latest evidence is in CURRENT_STATE.
Logical acceptance is asserted synchronously. Native output assignment is checked
separately after one bounded wait: existing Bento placement can complete after
the call returns. The test does not issue corrective geometry or retry placement.

DesktopStageController now prepares opaque receiver reservations with arrival,
output identity/geometry, usable area, application generation, layout presence,
and ordered resident membership. Copies share a one-attempt consumption flag.
The prepared-transfer API checks the reservation before solving and again before
source commitment. Changed/replaced targets reject without source removal. Bento
proposal solving in this path does not invalidate live application state; commit
still issues the normal application generation before native placement.

Private Active departure tests use this receiver API for native and existing-Bento
arrival, invalidate a reservation before a rejected drop, and attempt replay.
This is conservative: unrelated controller invalidation can require a fresh preview.
It reserves receiver state, not an arbitrary stack insertion slot or solved pixels.

NativeCarryHandoff now binds a preview ticket/semantic destination to synchronous
receiver validation and commitment callbacks. Only the owning physical release
can produce that request. It compares the exact ticket, origin and destination,
checks cancellation generation around receiver validation, and resolves the carry
before committing. Resolution disconnects old source observers before placement
changes outputs. The transaction still validates both controller reservations.
Copies of the source record survive only through the synchronous transaction;
commit rejection reports ReturnToOrigin, not successful placement. Reentrant
staging/release is blocked; cancellation remains available. Callbacks must not
destroy the coordinator synchronously.

Private real-pointer/touch tests now connect correlated native adoption through
physical release to actual Active-to-native transfer. They cover foreign release,
stale receiver generation, cancellation inside validation, rejected commitment,
movement invalidating the preview, and one-shot outcomes/commit. Destination
assignment is checked after a bounded wait, including after source cleanup.

Production preview recognition must bind the right controller reservation to its
semantic destination; this API does not infer edge/slot intent. Existing-Bento
admission remains tested separately, not through this gesture-release test.
DesktopStageController::transferNativeCarryToDesktop now connects a Bento source
reservation and PreparedDrop to the existing Bento handoff transaction. It checks
both reservations, exact arrival identity, a different output, completed native
movement and intersecting target geometry, then consumes the receiver reservation.
Open-space native and existing-Bento destinations reuse their existing value-copy
departure/admission and publication; no duplicate geometry path was introduced.
These proposal planners no longer invalidate live application state. Rejected
proposals therefore retain a usable source reservation. ActivateBento is explicitly
rejected by this adapter until edge-intent routing is integrated.

The same physical-release probe now covers Bento sources with pointer and touch:
retired receiver reservation, cancel during validation, rejected commitment,
movement-invalidated preview, successful native arrival and duplicate rejection.
An existing-Bento destination is also checked through the prepared controller API,
including source-session removal, destination membership and physical output.
That existing-layout check is not yet the gesture-release route. Tablet reception,
full Effect lifecycle, aperture and input ownership remain gated.
No install, push or repair promotion.
