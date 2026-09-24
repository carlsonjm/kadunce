# Integration release gate

This gate covers the implemented Kadunce carry and restoration routes. It is source
validation. Installation, live safety, and physical acceptance remain separate.

## Private route matrix

Run from the repository root:

```bash
bash tests/verify-integrated-carry.sh
```

While iterating on one change, name only the scenes it touches:

```bash
KADUNCE_GATE_SCENES="output-unplug-runtime desktop-bezel-runtime" bash tests/verify-integrated-carry.sh
```

A candidate is handed over only after a run with every scene. The full run takes
about three minutes; `KADUNCE_GATE_JOBS` sets how many private compositors run at
once (default 4). Every scene runs even after one fails, and each failure names
its log.

The harness snapshots the exact source and tests, records the base revision and
hashes, builds the production plugin, a disposable virtual-tablet variant and
the probe once, and runs:

- all native CTests;
- pointer and touch cross-output carry and destination footprints;
- same-output Bento exchange, return-home, dock exclusion, and restoration;
- ordinary monitor entry, edge withdrawal, cancellation, unload, and restoration;
- tablet Active departure and Bento return into Spread;
- a monitor unplugged and plugged back in, leaving every card and its window on
  the tablet;
- Spread transfer, stack insertion/browsing, cancellation, and interrupted motion;
- candidate Bento transfer/restoration/unload and independent safety checks.

Each runtime suite uses a private display, runtime directory, and D-Bus session. The
live safety check is read-only. The harness does not install, inject input into the
real session, toggle the effect, restart Plasma, or execute repair. Keep its printed
evidence directory until the candidate is accepted or rejected.

## Promotion checks

A promotable candidate requires all of the following:

1. Exact source identity and clean expected diff.
2. Production build and focused native tests pass.
3. `tests/verify-source.sh`, `tests/verify-package.sh`, and
   `tests/verify-control.sh` pass.
4. The integrated private route matrix passes without weakened assertions.
5. After authorized installation, installed binary provenance and
   `tests/verify-live-control.sh` pass.
6. Physical review covers the behavior changed by the candidate plus restoration,
   hardware touch, output scaling/topology, and instant disable when relevant.

## Known open boundaries

- Native-to-stack must admit membership and exact insertion atomically; current
  native arrival and existing Spread insertion are separate concepts.
- Displaced Bento neighbor, some tablet arrival feedback, and tablet receiver outline
  remain presentation gaps.
- Physical appearance, frame pacing, fullscreen release, fractional-scale transfer,
  suspend/resume, and hardware input cannot be accepted from the private harness.

## Stop conditions

Any safety-control, source-identity, restoration, teardown, or isolated-route failure
blocks promotion. Infrastructure failure remains infrastructure evidence until the
private environment is proven healthy. Do not patch production behavior to compensate
for an unclassified sandbox, display, D-Bus, or dependency failure.
