# Integrated carry release gate

This is the current finish line, not a new feature wishlist. Source-only;
installation and physical acceptance remain separate. See CURRENT_STATE.md for
the installed/repair identities and current evidence.

## Reproduce the implemented-route checks

Run `bash tests/verify-integrated-carry.sh` from the repository. It snapshots native
source/tests (including uncommitted work), records hashes and base commit, builds
production and an explicit disposable Virtual-0 tablet variant, then runs:

- All native CTests.
- Real-plugin pointer/touch cross-output carry, destination footprints, unload.
- Same-output Bento pane exchange, return-home, dock exclusion, restoration.
- Ordinary monitor entry: pointer/touch edge creation, withdrawal, held unload and
  restoration, including a cross-monitor edge destination.
- Tablet Active departure and Bento return into a resident Card Line.
- Card Line transfer, stack insertion/browsing, cancellation, interrupted motion.
- Full candidate Bento transfer/restoration/unload and independent safety checks.

Each suite runs on a private display/bus. The live safety check reads registration
only. No install, real input injection, effect toggle, restart or repair execution.
Failure stops the gate. Logs and both binaries remain in the printed temporary
evidence directory. Passing is NOT physical appearance/frame-pacing acceptance.

## Remaining implementation boundaries

1. **Ordinary desktop entry:** now connected for eligible monitor windows using a
   controller-owned pickup snapshot, existing contact proof, receiver planner and
   one-shot placement. Edge withdrawal, cancellation and unload are tested; native
   resize/unsupported starts remain native. Physical cross-display acceptance is
   still required. The opt-in checklist is CARRY-TEST-TONIGHT.md.
2. **Native-to-stack:** native receiver selection represents tablet arrival, not a
   stack slot. Card Line's prepared insertion handles an already admitted selected
   member. External membership admission and exact stack insertion must publish
   together before placement, with preview identity/slot validation. Appending an
   arrival and then trying a second stack command is not atomic acceptance.
3. **Remaining motion:** displaced Bento neighbor, Active/inactive tablet arrival
   feedback and tablet receiver outline. These are presentation work, not reasons
   to change native geometry repeatedly or extend input ownership.
4. **Physical acceptance:** quick flicks/holds, interrupted animations, live buffer
   rounding, Tette, fullscreen release, fractional-scale monitor handoff and instant
   disable. Preserve the stable installed build until an explicitly approved test
   candidate and recovery path are ready.

## Stop conditions

Any safety-control, source-identity, restoration or isolated-route regression blocks
candidate promotion. Do not report the full overhaul complete while entry/stack
routes above are missing. No new renderer rewrite, persistence service or timing
redesign is required merely to run this gate. Persistent stacks across complete
release remain a separately documented limitation, not silently added scope.
