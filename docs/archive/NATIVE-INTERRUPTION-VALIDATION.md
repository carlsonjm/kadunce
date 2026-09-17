# Native interruption validation — 2026-09-11

Status: targeted isolated tests pass. No production/runtime source changes in
this validation turn; no installation, publication or repair promotion.

## Real callback interruption

`KADUNCE_PROBE_SESSION=bento-session.sh bash tests/verify-unload-isolated.sh`
builds a non-installable probe containing the current DesktopStageController,
BentoLayout and a test-only host inside private virtual KWin. A real Qt Wayland
client is minimized before admission. Its synchronous minimizedChanged callback
calls restoreAllSessions during the unminimize setter in native placement.

Assertions require the callback to actually fire once, the controller to have
no remaining session, and the client to remain minimized. After 600 ms (beyond
the old settling window), geometry equals the recorded original and minimization
still holds. A fresh Bento activation/restoration must then succeed. No event
pumping occurs inside the callback. This exercises production controller code,
not a replacement model or fake client; the host classification is test-only.

Evidence: `/tmp/kadunce-unload-test.p7R33i/session.log`, explicit PASS marker.
Probe mapping is verified against its private build. Never load this probe in
the live desktop or package it with Kadunce.

## Full candidate two-output regression

`bash tests/verify-bento-candidate.sh /tmp/kadunce-block1-build` loads the actual
candidate plugin into a private two-output compositor. The existing Bento script
passes ordinary-output transfer, managed-to-managed transfer, source depletion,
destination membership, session restoration and full-effect unload assertions.

Evidence: `/tmp/kadunce-bento-candidate.TGwKWb/session.log`.
This test asserts session counts/operation success; it does not measure every
native configure or compare all post-transfer client geometry.

## Boundaries

### Restore guard follow-up

Bento restoration now checks weak client/window/output validity between native
setters and rejects new activation/admission while restoring. The shared
fullscreen/tile/maximize restoration order is preserved. Active's existing
wrapper remains unchanged; it does not yet use the checked predicate.

WindowHandlingTest interrupts each of nine exercised restore mutations, requiring
no subsequent mutation. A real KWin minimizedChanged callback now attempts Bento
admission during restoration: admission is rejected, restoration completes, and
fresh admission after the restore scope succeeds. Both this case and the earlier
placement interruption pass in `/tmp/kadunce-unload-test.xiU9ik/session.log`.
The rebuilt full candidate two-output regression passes in
`/tmp/kadunce-bento-candidate.zEjr2Q/session.log`. Eleven native tests and
source/control/live safety checks pass; installed and repair hashes unchanged.

Router teardown regression also passed with the rebuilt probe:
`/tmp/kadunce-unload-test.Nx6QFD/session.log` (held pointer/touch, mixed ownership,
recreation and fresh input). Eleven native tests and source/control/live-safety
checks pass. Installed plugin and trusted repair hashes match CURRENT_STATE.

This does not prove every setter-triggered lifecycle case. Physical hot-unplug,
window destruction during restore, effect destruction while a setter callback
is on-stack, and clients refusing/delaying configure still need targeted checks.
The restore guard stops unsafe continuation; it does not retain/retry an
interrupted snapshot on another output or recover refused geometry. Card Line/tablet/native
handoff paths still need transaction integration. These passes do not make the
unfinished remodel install-ready or establish touch smoothness.

Temporary paths are evidence locations, not durable caches; recheck existence.

## Bounded geometry-failure fallback

Historical first slice; the reconcile-once follow-up below supersedes retries.

The settling loop now gives its last corrective resize one extra 90-ms interval
before a read-only geometry/output/minimization check. Persistent mismatch
restores only that output's session. Normal successful settling still exits
early. Restoration uses the guarded snapshot path; its own interrupted-output
retry remains unresolved. A client slower than this bounded window may fall back
to desktop state; this does not establish a protocol-level refusal signal.

Evidence: `/tmp/kadunce-unload-test.HjmY7Z/session.log`. Test-only native mutation
every 10 ms forces repeated geometry mismatch on a real Wayland client. Recovery
requires actual contention, no remaining session, exact original frame geometry,
non-minimized state, and fresh activation. A normal session stays managed past
800 ms, ruling out unconditional timeout restoration. Earlier callback/reentry
tests also pass. This injection is NOT a real noncompliant-client fixture: an
attempt using QWidget fixed-size hints did not force refusal and was removed.

Full candidate two-output regression passes:
`/tmp/kadunce-bento-candidate.yGH9t2/session.log`. Eleven native tests,
source/control/live safety checks and whitespace checks pass. Installed plugin
and trusted repair hashes unchanged; no install or push. Remaining: output loss
mid-restore, actual delayed/noncompliant clients, and remaining transfer adapters.

## Reconcile-once follow-up

Production settling now has one 450-ms single-shot observation, no geometry-only
application path and no resize retry counter. New semantic placement still runs
the checked native sequence once. A mismatch at observation restores that output
session once; accepted geometry is untouched. Native interaction stops the timer.

`/tmp/kadunce-unload-test.wWf2Qn/session.log` passes real callback interruption,
restore reentry, injected contention/recovery, fresh activation and normal-layout
retention. The contention fixture now samples every 50 ms and additionally requires
zero geometry reassertions between injected moves (before fallback). This checks
observed behavior, not every possible native setter invocation or late configure.
`/tmp/kadunce-bento-candidate.3Fxb3D/session.log` passes two-output regression.
Eleven native tests, source/control/live safety and whitespace checks pass.
Installed/repair hashes unchanged. Output-loss recovery remains the next task.

## Mid-restore topology recovery

RestoreOnSurvivingOutput advances only after output loss, over a deduplicated
candidate snapshot; native user takeover/closure aborts. The production restore
loop retains its original snapshot, checks retired/live outputs between setters,
and clamps fallback coordinates. No repeated sizing requests on the same output.
WindowHandlingTest covers output loss followed by success, unavailable candidates,
abort without retarget, every candidate disappearing, and none available.

`/tmp/kadunce-unload-test.UimGKY/session.log` uses two virtual outputs and injects
the controller's removal notification from a real client's minimizedChanged
callback during restoration. The supposedly removed output deliberately remains
in KWin's list. Assertions require the callback, migration to a different output,
preserved minimized state, no lingering session and fresh admission afterward.
Earlier interruption, contention and stable-layout checks still pass.
This is NOT physical output destruction or unplug; client geometry after a real
hotplug, all-output disappearance and output replacement remain hardware/lifecycle
validation gaps. If no candidate survives, the loop warns and defers placement to
KWin instead of retaining a deferred retry record.

Full candidate regression: `/tmp/kadunce-bento-candidate.ObPbDu/session.log`.
Eleven native tests, source/control/live safety checks and whitespace checks pass;
installed plugin and trusted repair hashes unchanged. No install or push.
