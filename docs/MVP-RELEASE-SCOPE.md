# MVP scope reset — September 12, 2026

J: the build is stable and meets most MVP requirements. Finish dock safety and
visible new-app Bento admission together, then physical acceptance and freeze/push.
This overrides the mandatory K0–K6 sequence; CURRENT_STATE owns installed status.

## This batch only

September12 evening: J accepted the installed edge-stabilization candidate and
authorized freeze/push as new main. See FREEZE-20260912-EDGE-STABILIZATION.md.
The earlier swipe/Konsole feedback motivated the accepted candidate, not a
standing freeze blocker. Do not roll back accepted transfers or rebuild KWin
speculatively. The completed batch below does not authorize a new feature packet.

- Dock-safe bottom release on monitor AND tablet-only: ordinary window recovery
  and explicit Bento departure, including floating docks without work-area struts.
  Preserve ordinary dock input and cancellation; one release-time correction.
- New eligible apps receive visible Bento space through the existing planner,
  not unconditional minimization into overflow. Preserve original restore state
  and output-local ownership. If admission cannot fit, keep the newcomer visible
  and native rather than hiding it or disturbing the accepted layout.
- Focused checks, independent safety control, provenance/rollback, J acceptance,
  then the requested freeze/push. No unrelated features before this checkpoint.

## Remaining correctness questions, not automatic refactors

Post-freeze J update: occupied-tablet arrival is considered solved. Affinity's
awkward splash is explicitly deferred compatibility polish; the main application
opens and accepts card state. Neither is active MVP work. J assigned stack
placement and related motion next; the numbered notes below retain history.

1. Occupied-tablet arrival: an earlier failure exists alongside later transfer
   passes. Reproduce that exact occupied-target case on current code when hardware
   is available before authorizing K2's transaction changes. Current failure is
   unconfirmed; existing architecture is not presumed missing.
2. Slow/helper launch readiness: ordinary new-app admission is in this batch.
   Affinity's splash/main-window behavior needs a bounded real check afterward.
   Steam discovery/external storage is separate feature work, not this fix.
3. Any current lost window, stuck input or broken disable is a blocker. Existing
   recovery is implemented and tested; fix evidenced failures, not a speculative
   rewrite. Checkpoint freeze does not claim universal hardware coverage.

## Deferred experience work

Card Line stacking and a shared insertion model already exist. Additional native
stack producers and left/right slot tuning are improvements, not prerequisites
to this freeze. Flick/regrab/settle polish should target an observed discontinuity
J wants fixed, not blanket timing changes or a replacement motion engine.
Persistent layouts, new tiling modes, renderer extraction and always-on services
remain deferred. Architectural cleanliness does not block a usable checkpoint.

## Separate app releases

Tette Meta toggle, guest sizing/fullscreen dock access, Steam discovery and All
Files; Temperance notification/power/banner work: independent releases. B owns
the assigned Temperance work. Old E/A lists are inventories, not fresh bug reports;
recheck each app when its packet is assigned. None expands this Kadunce batch.

## Planning-time resource discipline

A owns engineering scope and cost. Before editing, name the user-visible failure,
existing owner, smallest correction, focused checks, J's physical test and stop
point. Expected configurations (tablet-only, dock on either display, floating
dock) belong in that invariant without requiring J to enumerate them.
Focused checks plus mandatory safety are the default; J owns broad physical
regression. Do not rebuild unchanged components or rerun every gate. An upstream
repair, large build, new owner or expanding matrix requires explaining the scope
and cost change and getting direction first. Two failed approaches trigger a
short evidence review, not a third speculative workaround. Completion means the
agreed experience, not accumulated test counts.
