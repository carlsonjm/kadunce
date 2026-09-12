# Restoration investigation checkpoint — 2026-09-11

Historical checkpoint. Resumed candidate and green regression results are recorded
in [RESTORATION-VALIDATION.md](RESTORATION-VALIDATION.md); do not treat the red
status below as the latest state. The original source checkpoint remains intact.

Development freeze, NOT an install-ready or all-green release. User requested
restoration followed by a freeze. Investigation isolated the failing interaction;
the production fix remains unfinished. No install, repair promotion or push.

## Reproduction and finding

The private virtual KWin test client starts maximized at 1280x800. Plain KWin
calls unmaximize it and resize to 1260x770. After settling, maximize and minimize
in the same call. Showing it again leaves frame/requested geometry at 1260x770.
This control does not invoke either Kadunce controller, its restoration helper,
CarrySession or NativeMoveTakeover.

Protocol evidence shows a 1280x800 configure, acknowledgment and matching buffer,
but no corresponding new xdg_surface window geometry; bounds remain 1260x770.
The client/compositor interaction is established, not which upstream component
should own the fix. This is not evidence of a decoration radius or gutter bug.

- Plain native failure: `/tmp/kadunce-unload-test.M8sON0/session.log` (Wayland trace).
- Controller no-drag failure: `/tmp/kadunce-unload-test.POW39k/session.log`.
- Controller protocol trace: `/tmp/kadunce-unload-test.reGHW7/session.log`.
- Positive separated-minimize control: `/tmp/kadunce-unload-test.LtNgz5/session.log`.

The positive control waits in the TEST harness, asserts exact restored frame
geometry, then minimizes/shows. It passes. This is not a production timer fix.
Changing to requested-state guards and moving minimization earlier both failed
to solve it; experimental production changes were reverted.

## Re-run

From the repository:

```sh
# Existing controller regression: still expected to fail, must not be waived.
KADUNCE_PROBE_SESSION=bento-session.sh bash tests/verify-unload-isolated.sh
# Native failure control (no Kadunce controller calls).
KADUNCE_RESTORE_NATIVE_CONTROL=1 KADUNCE_PROBE_SESSION=bento-session.sh bash tests/verify-unload-isolated.sh
# Positive control, geometry verified before separate minimization.
KADUNCE_RESTORE_NATIVE_CONTROL=1 KADUNCE_RESTORE_SEPARATE_MINIMIZE=1 KADUNCE_PROBE_SESSION=bento-session.sh bash tests/verify-unload-isolated.sh
```

## Next bounded task

Design acknowledgment-gated minimization with explicit lifetime ownership before
writing the workaround. Restore geometry/state once; minimize only after the
restored presentation is acknowledged. Cover close, manual takeover, output
loss, re-entry, disable and effect unload. Plugin-owned deferred callbacks cannot
outlive the unloaded plugin. A timeout is not proof of correct geometry; do not
add repeated resize requests or relax the exact visible-frame assertion.

Then rerun controller normal/minimized/maximized/fullscreen cases, interruption
and full-effect unload checks. Only after those pass resume live native carry.

## Verified freeze boundary

Candidate builds; eleven standard tests and source/control/live safety checks
pass. Existing initializer warnings remain. The expanded private restoration
suite is red, so candidate installation remains blocked. Installed plugin hash:
`6b752de4f4d9cedbf5ae533716c93135d6cf7c1b003ba2751df27b067a961448`.
Repair source hash:
`27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9`.
Both unchanged. Monitor hardware testing remains parked.
