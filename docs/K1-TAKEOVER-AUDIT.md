# K1 takeover audit — September 12, 2026

Follow-up: the owner-lifetime repair is now privately validated. See
`../patches/kwin/README.md` and CURRENT_STATE.md. The investigation below records
why the earlier approaches were rejected; it is not the latest completion status.

J asked A-Team to take over. B was instructed to stop edits and tests; its task
is idle. No installed software, desktop session, repair archive or Git remote was
changed. This is a failing-regression checkpoint, **not an update ready to install**.

## What changed in the assessment

B's proposed DesktopStage settle-timer retry did not have a discriminating
before/after regression. The accepted f308 binary passes the multi-card test
that was intended to justify it. A withdrew that production change and preserved
the whole prior native/tests tree in `/tmp/kadunce-k1-a-takeover`.

The sequence-sensitive touch failure is real. The earlier fixture reused contact
IDs 71/72 and checked logical membership, not whether ordinary movement followed
the contact. That can mask a stale input identity. It also only checked Kadunce's
inputBusy, not KWin's still-running native move after the release comparator.

Current tests retain all existing membership/preview checks and add:

- Different touch IDs across scenarios.
- A changed native position after ordinary movement, before intentional edge entry.
- Native move completion after the Ctrl+Esc-release comparator lifts its contact.
- PASS text naming platform, selected input kinds and scenarios; a focused run
  no longer claims that the full pointer/touch matrix passed.

## Source-level explanation

Matching primary source: [KWin v6.7.5 input.cpp](https://invent.kde.org/plasma/kwin/-/raw/v6.7.5/src/input.cpp),
downloaded read-only to `/tmp/kadunce-kwin-input-6.7.5.cpp`.
MoveResizeFilter is at lines 666–805. installInputEventFilter is around line 2948.

MoveResizeFilter stores `m_id` and `m_set` on the first native touch motion.
Kadunce can later take over at an edge and end the native move while that touch
remains held. The carry owns the physical release. KWin's filter has no native
finish or touch-cancel reset; even a touchUp with no moving window returns before
resetting its saved ID. The next native move with a different ID therefore:

1. Swallows motion without updating the window because the saved ID differs.
2. Clears its flag on the mismatched release but does not finish the native move.
3. Can swallow the following touchDown before the client can request a fresh move.

This matches the observed motion/release/proof sequence. It is not proof that
every reported pointer re-entry failure has the same cause.

The proposed filter-priority change is rejected. Installed `input.h` orders
VirtualTerminal, LockScreen, ScreenEdge consecutively; ScreenEdge-1 equals
LockScreen. `lower_bound` inserts a newly registered equal-weight filter BEFORE
existing filters, so Kadunce at ScreenEdge already precedes the older native
screen-edge filter. Moving it earlier is both unsafe and irrelevant to this defect.

## Evidence ledger

| Evidence directory | Exact meaning |
| --- | --- |
| `/tmp/kadunce-unload-test.PCDjfb` | Accepted f308 passes pointer/touch multi-card departure and re-entry; timer fix not established |
| `/tmp/kadunce-unload-test.ZTDWfi` | B f682, touch detach then release; native move still active after actual up, before subsequent edge attempt |
| `/tmp/kadunce-unload-test.gZ0J8H` | A release-only fallback passes the older focused assertions; not sufficient |
| `/tmp/kadunce-unload-test.PBT428` | Same experimental fallback passes older full Wayland exit assertions; not sufficient |
| `/tmp/kadunce-unload-test.5ct93i` | Same fallback fails stronger native-movement assertion; workaround rejected |
| `/tmp/kadunce-unload-test.ZW76hT` | Accepted f308, touch detach twice with distinct IDs; first native motion changes position, second does not |

A's rejected fallback let the real up pass to KWin, then queued a guarded native
finish if it remained stuck. That can close the move but cannot restore motion
already lost. It is removed from production. Its header is preserved in
`/tmp/kadunce-k1-a-takeover/NativeCarryRuntime-release-only-rejected.h`.
Its binary remains in `/tmp/kadunce-k1-build.lIvfKO`, SHA256
855c1c55cd51326bade5fd182d892ad33f1ff0a20e1be040ad729d5ec24e2d2a.
Do not install it or rebuild that evidence directory under the same identity.

## Small reproducer, no live session involved

From the Kadunce repo, using the verified accepted disposable build:

```bash
KADUNCE_EXIT_KINDS=touch KADUNCE_EXIT_SCENARIOS='detach detach' \
KADUNCE_RUNTIME_BUILD=/tmp/kadunce-integrated-carry.NizPos/production \
KADUNCE_PROBE_SESSION=exit-runtime-session.sh bash tests/verify-unload-isolated.sh
```

The existing harness isolates all sockets/configuration and never installs.
If this disposable build is gone, verify another accepted build rather than
silently substituting a candidate. Socket approval does not authorize live input.

## Next bounded engineering decision

Investigate a reset tied to native move lifetime at the owner of that state.
Preserve KWin's ordinary movement, exact contact correlation and lock-screen
ordering. Do not ship synthetic releases, priority hacks, a release-only patch,
or another geometry controller just to green the gate. An upstream/native fix
must be separately assessed for packaging/compatibility and explicitly approved
before changing the user's KWin installation.

Acceptance must cover repeated different contact IDs, ordinary native movement
before the edge, exact release, canceled takeover, window closure, fresh input,
held disable/unload and Wayland/Xwayland. Revisit the original pointer K1 report
separately if this correction does not reproduce it. Then run one full fresh
candidate gate and ask J for the physical loop. No new J test is requested yet.
