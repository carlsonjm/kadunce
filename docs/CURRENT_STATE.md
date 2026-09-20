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
- A Bento layout shows every window it owns awake. A solve that cannot place
  one is refused rather than parking it, and the window becomes an awake
  individual card on the display that can hold one. A launching application
  joins a layout that can grow to show it beside its existing panes, and is
  left to card ownership otherwise.
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
duplicates, and omits stack position.

## Open limitations

- A pane the user minimizes stays owned by its Bento session rather than
  becoming a sleeping individual card. `CARD-LIFECYCLE.md` §7 gives it to card
  ownership, but a minimized window is not an eligible card window, so the card
  stage cannot adopt one; the session keeps its restore record instead and
  carries it across the Spread round trip as a sleeping member. It is the only
  window a session owns without showing, and `applySession` reports any other.
  §5's one-remaining-pane rule is also not implemented, so a two-pane layout
  losing a pane holds one rather than ending into card ownership.
- A projected Bento group resumes by restoring unrelated Spread neighbors. This
  is superseded by the approved visible-pane ownership contract and requires the
  same ownership work as top-edge extraction before that can be promoted.
- Where no display can hold a card, a window a layout cannot show is not
  adopted at first entry and keeps its own place on the desktop. Where one can,
  a window leaving a live layout reaches it through the same adoption a carried
  card uses, so the card stage presents Spread when it was not already
  presenting. Neither is stated by the contract; both are recorded in
  `DECISIONS.md`.
- Card ownership is structurally single-display. One card workspace exists and it
  is bound to one output, resolved as the internal panel, so
  `CARD-LIFECYCLE.md` §11's independent per-display ownership session holds on
  that output only. Another display can hold its own Bento session, but it cannot
  hold individual cards, Spread or an Active card; a card carried onto one
  becomes a Bento pane or an ordinary desktop window there.
  `PRODUCT-CONTRACT.md` owns whether an external output ever presents cards; this
  entry records only what the current structure does. It is also why a window a
  layout cannot show moves to the display that can hold it as a card rather than
  staying where it was.
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
no-member-paging, sleeping-member, residue-free repeated exact resume,
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

The ownership observer has been live-verified on a restarted compositor
running the installed candidate. On that candidate it reported the retained
Bento remainder and no other violation, suppressed an unchanged shape, and
reported nothing during tray disable, release and re-enable. The remainder it
reported no longer exists in source and the rule that named it is retired, so
the observer now holds two rules; that has not been measured live.

## Safety

`AGENTS.md` owns the safety requirements and the checks a change must run.
Current status: the mandatory control checks pass for the accepted sources and
again on the installed candidate in the graphical session, where the tray
enable/disable control released and restored ownership cleanly.

Ordering, task detail and open product decisions live only in `ROADMAP-CC.md`.
