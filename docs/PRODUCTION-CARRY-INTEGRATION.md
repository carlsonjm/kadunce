# Production carry integration — 2026-09-11

The real Effect now consumes the prepared native-carry pipeline. This is no
longer only a private controller probe. It is an uninstalled, limited candidate,
not completion of the unified interaction overhaul.

## Runtime ownership

Effect owns NativeCarryRuntime: passive NativeMoveObserver, NativeCarryHandoff,
and CarryInputRoute. A controller prepares the source before native movement;
only a correlated, still-held contact permits takeover. Synchronous native
cancellation is not treated as a completed drop. Rejected admission retains the
legacy native path. Lock-screen/VT handling precedes the carry input filter.

Only the admitted window follows the frozen pickup rectangle through compositor
transforms. Native geometry is not rewritten on every motion. Per-output carry
clipping is separate from the unchanged passive tablet-card fence. A receiver
reservation is validated again at release before the existing controller
transaction applies placement. Cancellation clears presentation and drains held
contacts; Effect teardown removes runtime ownership before restoring controllers.

## Connected scope

- Source adapters: Active and Bento prepared native move.
- Destination: another output, open desktop, existing Bento, edge-created Bento,
  or Bento-to-tablet arrival through the existing receiver transaction.
- Native resize and unsupported/unproven starts retain existing behavior.
- Same-output reorganization and stack insertion are not connected to this release
  path yet. Existing Card Line routing remains.
- Top/left/right edge intent is recognized during motion within 12 logical px.
  The bottom 48 px and panel hits are excluded. Release consumes that preview
  ticket; moving away invalidates it. Existing Bento takes priority; new Bento
  promotes the destination's eligible residents through the batch planner.
- No new animated destination preview or motion polish is claimed.

## Evidence and boundaries

`tests/unload-probe/runtime-session.sh` loads the actual production plugin in a
private two-output KWin; the probe supplies input, not substitute controllers.
Pointer/touch native pickup, destination release and unload while held pass.
The remaining physical release produces no extra client action.
Open-space, edge-created Bento and withdrawn-edge cases pass for both input types.
Evidence: `/tmp/kadunce-unload-test.yhcu3u/session.log`.

Full candidate two-output transfer/restoration/unload passes:
`/tmp/kadunce-bento-candidate.eQTnIm`.
All 14 CTests, source checks, control package checks and read-only live safety
control verification pass. Installed binary and repair archive hashes unchanged.

An initial private run crashed after eager offscreen redirection on adoption.
That eager call was removed; the existing shader-aware draw path now owns
redirection. The virtual compositor validates fallback rendering, not hardware
shader quality or frame pacing. Hardware visuals remain unverified.

The ordinary virtual-output run covers Bento sources. A disposable copy at
`/tmp/kadunce-tablet-runtime.O8dWSS` changes only the output classification
predicate to recognize Virtual-0 as a panel, following the earlier unload fixture.
Its full Effect passes Active pickup, open/edge monitor departure and tablet return
for pointer/touch: `/tmp/kadunce-unload-test.vB5joN/session.log`.
Production classification is untouched. This is not hardware acceptance.

That fixture exposed an Active entry bug: late reported maximize state was treated
as a new manual command. Active now uses requested maximize/fullscreen/quick-tile
state for intent, while native start alone owns move/resize admission. This prevents
geometry notifications from releasing the stage before adoption can run.

## Next acceptance boundary

Unify Card Line/stack insertion/browsing and provide destination visuals. Preserve one-time native reconciliation,
passive tablet cutoff, dock behavior and immediate disable. Only after that is
the complete interaction path ready for motion/physical testing and freeze.
