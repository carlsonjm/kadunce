# Carry session contract

This reference defines the live transfer boundary shared by Spread, Bento, and
ordinary native desktop movement. `ARCHITECTURE.md` owns the broader authority model.

## Session value

A carry session is a one-shot, revision-bound value that identifies:

- the source window and controller;
- authoritative restore state captured before presentation ownership changes;
- the initiating input device/contact and pickup pose;
- source membership, stack order, selection, and generation;
- current topology/output generation;
- a semantic destination, every window identity that destination names, and any
  prepared destination plan.

The session contains no independent mutable window model. Presentation may derive a
carried pose from it but cannot commit membership or geometry.

## Lifecycle

1. Prove the exact source and initiating native/card interaction.
2. Create a reversible source reservation without removing membership.
3. Recognize a semantic destination, including every window identity it names, and
   prepare it on a value copy.
4. Revalidate source, destination, named identities, topology, visibility, and
   generations.
5. Commit source removal only after destination acceptance.
6. Publish destination state, release input ownership, then apply guarded native
   placement when the destination uses real geometry.
7. Retire the session exactly once.

Cancel, source closure, output loss, manual takeover, new input, or failed
revalidation retires the session without stale callbacks. Before destination
acceptance, cancellation restores exact source state. After logical commit,
interruption is consumed as destination-owned state and cannot resurrect the source.

## Destinations

### Existing Bento

Existing output-local Bento has priority. Admission checks duplicate membership,
minimum sizes, visible placement, output revision, and current session revision.
Rejection does not fall through to ordinary desktop placement.

Same-output pane exchange swaps identities and restore records through one prepared
layout. Returning to the original pane consumes the drop without redundant native
placement.

### New Bento

Where a deliberate edge destination means a pair, a new layout begins only from
the pair the gesture named. Preparation admits exactly the carried window and
that one partner, so no resident batch is collected: nothing else on the display is an input to the solve, the preview, or
the revalidation, and a prepared plan that is not exactly those two panes is
rejected. A display the pairing grammar does not reach still prepares the
eligible resident batch plus the arrival. Which windows may be named is
`CARD-LIFECYCLE.md`; this document does not restate it.

Preparation does not invoke a mutating shortcut path. Unrelated clients, panels,
other outputs, and companion guests are not recruited.

### Spread

A top, left or right edge destination on a display Kadunce does not yet own
adopts the display instead of beginning a layout; `CARD-LIFECYCLE.md` governs
what adoption takes. Successful admission publishes membership before
placement/presentation cleanup. The arrival may seed the existing Spread
center/expand sequence from its released pose. Native-to-stack admission remains
incomplete until membership and exact stack insertion can be accepted atomically.

### Ordinary desktop

Open output space without an existing Bento remains native desktop placement.
Landing may receive one bounded work-area/dock clearance correction after native
finish. It does not resize the window or extend the carry route.

## Presentation boundary

The carried face is compositor presentation. It never becomes a second source of
restore geometry. Each output clips its own paint route. A committed native drop may
settle visually only while KWin's requested output and geometry match the reservation;
the settle owns no input and performs no native writes.

## Required invariants

- One session, one outcome, one source restore record.
- Preparation is read-only; commit publishes at most once.
- Destination acceptance precedes source removal.
- All generation and topology checks repeat at commit.

- Commit admits the exact identities the reservation named and revalidated; no
  destination identity is re-read at release.
- Rejection preserves membership, order, selection, and restore state.
- Native geometry is never written repeatedly during motion.
- Paint and preview cannot make acceptance decisions.
- No callback survives source, destination, controller, or effect teardown.

## Validation

Run `bash tests/verify-integrated-carry.sh` for the private route matrix. The matrix
does not replace hardware touch, live compositor, fractional-scale, frame-pacing,
restoration, or kill-switch review. Follow `INTEGRATION-RELEASE-GATE.md`.
