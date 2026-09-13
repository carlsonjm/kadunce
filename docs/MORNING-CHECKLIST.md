# Morning checklist — September 13 interaction-first revision

J: rollback completed. Basic Card Line interaction continuity is MVP-critical.
Geometry/animation work follows acceptance of these interactions, not alongside it.

September13 implementation checkpoint: interaction-only candidate71e5bf84 built
from isolated5ba19e6 under ../work/kadunce-interaction-20260913/source; not installed.
Stationary-entry test reproduced failure before fix. Focused router/state/model
tests3/3, source guards and control tests2/2 pass. Timer fixture uses real router
and membership model, with simulated compositor adapter. Sections below remain
the full acceptance checklist, not a claim that every item has been covered.
Next physical checks: stationary entry at both ends, deliberate middle insertion
and browse, physical-edge row paging then return to insertion. Geometry untouched.

## 0. Recovery status

- [x] Read repository instructions/current state; inspect branch/diff.
- [x] Confirm installed binary matches accepted38a2fbb5; main remains5ba19e6.
- [x] Verify independent live safety control is Active with kill switch exposed.
- [ ] Independently identify loaded binary if needed: KWin restarted09:20:18,
  but memory-map read was denied. J reports completed rollback.
- [ ] Preserve failed local stack experiment and bundles; do not build/reinstall
  that mixed experiment as the next baseline. Isolate accepted source plus the
  specific interaction fix without destructive reset or automatic publication.

## 1. Define and reproduce order continuity

- [ ] Separate membership order, selected member, and paint order. Frozen code
  appends by default and selects the newcomer; browsing follows stored order,
  not a recency sort.
- [ ] Reproduce the three-card case: start[A,B], placeC between them, expect
  [A,C,B] withC selected. Next reachesB; reverse returnsC. Activation must not
  rewrite[A,C,B].
- [ ] Test before-first, between-members and after-last insertion from either
  approach. Preview slot and committed index must describe the same location.
- [ ] Test moving an existing member, last-member detach and cancel. Cancellation
  restores original order/selection; accepted placement changes only that member.
- [ ] Browse forward/back, leave/re-enter Card Line, and activate another member:
  order remains placement-defined. Preserve existing wrap unless J changes it.

## 2. Separate insertion from held-row paging

- [ ] Reproduce stationary entry/end-seam dwell triggering unwanted navigation
  in the real router interaction test before changing its policy.
- [ ] Enter/hold over a stack must not request slot paging. Require fresh,
  deliberate navigation intent after insertion is armed.
- [ ] Reaching an end slot must not automatically turn into row paging.
- [ ] Held-row paging requires explicit physical-edge intent; every repeat
  revalidates contact, edge and current interaction state.
- [ ] Leaving edge, entering insertion, release, cancel or stale target stops
  repetition. Deliberate navigation stays reachable from both sides.
- [ ] Do not solve with blanket timer increases, app-specific exceptions, new
  input ownership or changes to dock/Bento/cross-display behavior.

## 3. Focused interaction candidate and J acceptance

- [ ] Test connected router/controller/model behavior, not only mathematical
  helpers. Cover stationary entry, deliberate slot changes, stale target,
  repeat stop, cancellation and persistent order.
- [ ] Build changed components; run source guards and mandatory safety checks.
- [ ] Package a separate byte-identified interaction-only candidate with rollback
  to38a2. No automatic install/restart/push or repair promotion.
- [ ] J: build/reorder three-card stack and browse it in both directions.
- [ ] J: hold over either insertion end—no involuntary stack/row navigation.
- [ ] J: deliberately page the line while holding, return to insertion, and cancel.
- [ ] Review acceptance before proceeding to presentation work.

## 4. Only after basic interaction acceptance

- [ ] Reproduce wrong overlapping corners in the actual Card Line state.
  Distinguish final geometry from retained/interrupted preview and clipping.
- [ ] Resolve canonical poses and size endpoints first, then animate them.
- [ ] Revisit44% held scale (J liked it) independently from order/navigation.
- [ ] Tune specific measured pickup/regrab/release/flick discontinuities, not a
  replacement motion engine or wholesale timing rewrite.

## Parked / stop rules

Occupied-tablet arrival is considered solved; Affinity splash is deferred.
B owns assigned Temperance work. Tette features, native-to-stack expansion and
distribution packaging remain separate. Two failed approaches require evidence
review, not a third combined speculative patch. No lost cards or stuck input;
independent disable remains mandatory.
