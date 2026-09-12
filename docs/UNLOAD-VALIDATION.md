# Unload validation — September 11, 2026

Status: isolated delivery/full-effect gate passed; physical tablet acceptance
not run. Candidate is uninstalled. Installed plugin and trusted repair hashes
remain those in CURRENT_STATE.md.

## Runtime change

Effect destruction now invalidates queued activation and cancels input while
controllers are alive, then explicitly destroys/unregisters the input router
before guest cleanup and window restoration. It no longer relies on reverse
member destruction, which previously destroyed controllers before the router.
The installed KWin input.h documents automatic filter unregistration by
InputEventFilter's destructor. No gesture timing or layout changes.

## Evidence

1. Nine native tests, source guards, control package tests and read-only live
   safety check pass after the teardown change.
2. The unmodified candidate passed the existing nested two-output Bento test:
   admission, transfers, restore and actual effect unload. No user-session toggle.
3. `tests/verify-unload-isolated.sh` builds a non-installable test plugin and a
   real Qt Wayland client inside virtual KWin with a private bus/config/runtime.
   It compiles the actual WorkspaceInputRouter against a test workspace target.
   A registered test device advertises touch/pointer capabilities. Baseline input
   must reach the client before zero orphan-release counts are meaningful.
   Pending/lifted touch teardown, lifted mouse teardown, recreation before release,
   mixed client/consumed contacts, forwarded pointer ownership and fresh input pass.
4. `tests/unload-probe/full-session.sh` exercised the complete candidate Effect
   and CardStageController. A disposable source copy changed **one predicate**:
   Virtual-0 is recognized as a tablet. Production output classification is
   unchanged. Plugin mapping was checked against that fixture's build path.
   Card Line was verified through workspaceContext; logs confirm real touch and
   mouse card lifts and their rollback before unload. Unload during a pending
   hold also passed. No orphan release reached the client; fresh input worked.

Full-effect fixture used Plasma-native system edges and the renderer's r20
fallback. It does not establish direct-Z13 edge routing, GPU corner quality,
physical touch latency, Tette's actual client behavior, or fullscreen visual
restoration. These remain in COUCH-VALIDATION.md; monitor hardware tests parked.
Synthetic events enter KWin's input pipeline; they do not emulate a physical
device backend's complete hardware lifecycle.

## Local artifacts (temporary; recheck before reuse)

- Full run/log: `/tmp/kadunce-unload-check.LLAVRb/full-verified.log`.
- Full fixture: `/tmp/kadunce-unload-check.LLAVRb/full-fixture/native`;
  build: `/tmp/kadunce-unload-check.LLAVRb/full-build`.
- Router-delivery run: `/tmp/kadunce-unload-test.fAs2yI/session.log`.
- Production-candidate Bento run: `/tmp/kadunce-unload-check.LLAVRb/bento-after.log`.

Early probe runs with no advertised input device were inconclusive and discarded
as evidence. The client also counts Qt double-click events as presses; otherwise
rapid valid clicks produce misleading release-only counts. Current assertions
require positive controls and a PASS marker, not merely compositor exit success.

No test probe is part of the native package or repair snapshot. Never install the
input injector or invoke its session scripts against the live desktop. The test
driver creates isolated directories, and session scripts require a virtual parent
compositor and verify plugin mappings. Logs/builds are retained for inspection.
