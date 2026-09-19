# Current state

## Product

Kadunce is the open-source spatial-window component used by Good Input's Shuffle
for Plasma product. KWin owns real windows and virtual desktops. Kadunce owns the
touch interaction model, card membership, output-local Bento sessions, input
routing, and compositor presentation. Table and Shuffle Keyboard are required
Shuffle capabilities; their technical feasibility remains open.

`CARD-LIFECYCLE.md` is the approved canonical ownership and presentation contract
and `ARCHITECTURE.md` holds the structural invariants. This document reports status
against both and does not restate them; a difference recorded here is a limitation,
not alternate behavior, and where wording conflicts the owning document governs.

## Accepted behavior

- Active, Spread, ordered stacks, and output-local Bento are implemented.
- `ARCHITECTURE.md` invariants 1-10 hold in the accepted sources except where
  `Open limitations` records otherwise.
- Constrained new windows enter the Bento solver or prepared Active ownership.
- Touch Spread preserves a Bento composition as one logical group card. Its
  pane-visible live surfaces keep their native work-area positions and proportions
  inside one centered desktop view, including outer gutters, pane gaps, and dock
  clearance. Painting reconstructs each pane from the transferred normalized rect,
  so later live-frame acknowledgements cannot move it inside the group. The stored
  pane frame supplies a proportionally scaled antialiased rounded aperture, keeping
  chrome inside while clipping expanded decoration and shadow pixels out of
  workspace gutters. One translucent-black field replaces wallpaper behind the
  panes and all group paint state retires before exact resume exposes native Bento.
  The group has no member paging, fan, or member carry; activation resumes the exact
  Bento session without a layout solve.
  Ordinary Spread cards retain their accepted presentation.
- Other outputs keep their own Bento sessions when the tablet changes presentation.
- Native carries preserve input ownership, exact restore records, dock clearance,
  cancellation, and output-local admission rules.
- First-layout edge admission re-solves current client size hints at commit. A
  still-feasible configure acknowledgement does not require a second snap;
  infeasible current hints still reject before ownership publication.
- Live card presentation uses KWin's off-screen texture path. Proportional margins
  are accepted; Kadunce does not retain a second snapshot cache.
- The persistent tray controller releases windows before unloading the effect and
  is wired to `graphical-session.target`.

The current `main` production sources and installed candidate include the accepted
ownership, constrained-launch correction, and physically accepted Bento group-card
presentation. Revalidate source, package, control, and live provenance before the
next installation.

The current local `main` adds physically accepted Spread application labels without changing
card geometry or input. Ordinary cards resolve their human application name from
desktop service metadata with window metadata and caption fallbacks, keep it
centered below the card, and put pageable stack position at the row's right edge.
A Bento group lists every visible pane application in pane order, including
duplicates, while excluding overflow and omitting stack position.

## Open limitations

- The current implementation still retains Bento overflow and restores unrelated
  Spread neighbors when a projected Bento group resumes. Both behaviors are
  superseded by the approved visible-pane ownership contract and require a protected
  ownership refactor before top-edge extraction can be promoted.
  Measured live on the installed candidate, two outputs attached: entering Bento
  on the tablet with six eligible windows produced three visible panes and three
  overflow windows, and none of the three held individual-card ownership.
  `CARD-LIFECYCLE.md` §14 gives every window outside the visible combination to
  the card stage, so each is one violation. The same measurement found no window
  with two owners, no display with two Bento layouts, and no effect on the other
  output, and it held unchanged across a tray disable and re-enable.
  Two user-visible symptoms follow. Activating an overflow window from the Plasma
  task manager does not bring it forward: un-minimizing re-solves the session and
  the solver minimizes it again. A pane dragged to the top edge returns to Bento,
  and ownership never records it leaving, so §5's departure and one-remaining-pane
  rules never run.
- Pulling a member back from a stack and reorder intent zones still need product
  completion.
- Native-to-stack admission is not one atomic destination transaction.
- Some arrival and displaced-neighbor transitions remain visually incomplete.
- Custom compositor motion does not yet fully follow platform animation scaling or
  reduced-motion preferences.
- Plugin installation assumes the tested native KWin plugin directory and requires
  a rebuild after a KWin ABI change.
- Table and Shuffle Keyboard have approved product contracts but no accepted
  feasibility implementation.

## Validation boundary

The production build, focused layout/motion/paint tests, integrated carry routes,
package checks, and mandatory control checks have passed for the current sources.
Ownership publication, native adoption and resume handback ordering is covered
behaviorally by the headless `ownership-handoff` test rather than by source-order
assertions; the invariants themselves are unchanged.
The accepted labels additionally have focused name precedence, Bento aggregation,
duplicate preservation, and stack-position coverage; their full verification passes.
The Bento group-card implementation has focused full-work-area, rounded
pane-aperture and repeated-projection geometry, translucent backdrop, session-contract,
no-member-paging, overflow-minimization, residue-free repeated exact resume,
first-snap refresh, and private two-output ownership lifecycle coverage. Physical
review accepted its geometry, tint, gutters, container-level rounded clipping,
exact resume, and repeated-entry behavior.
Physical review has accepted ownership, constrained launch routing, stack retention,
large-pane selection, monitor isolation, lifecycle, and the current live-rendering
model. Automated and private-compositor checks do not replace physical appearance,
frame pacing, hardware touch, fractional-scale, suspend, or live disable review.

Installing a candidate does not by itself put it in the running compositor.
`install.sh` leaves the effect unloaded rather than reloaded, and KWin keeps the
previous plugin image mapped across an unload, so a freshly loaded effect can
still be the previous build. A compositor restart is what replaces the image,
and until one happens an installed candidate is unexercised.

The ownership observer has since been live-verified on a restarted compositor
running the installed candidate. It reported the overflow violations above and
no others, suppressed an unchanged shape, and reported nothing during tray
disable, release and re-enable.

## Safety

`AGENTS.md` owns the safety requirements and the checks a change must run.
Current status: the mandatory control checks pass for the accepted sources and
again on the installed candidate in the graphical session, where the tray
enable/disable control released and restored ownership cleanly.

Ordering, task detail and open product decisions live only in `ROADMAP-CC.md`.
