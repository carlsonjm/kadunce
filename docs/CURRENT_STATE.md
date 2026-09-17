# Current state

## Product

Kadunce is the open-source spatial-window component used by Good Input's Shuffle
for Plasma product. KWin owns real windows and virtual desktops. Kadunce owns the
touch interaction model, card membership, output-local Bento sessions, input
routing, and compositor presentation. Table and Shuffle Keyboard are required
Shuffle capabilities; their technical feasibility remains open.

## Accepted behavior

- Active, Card Line, ordered stacks, and output-local Bento are implemented.
- Card Line is compositor presentation and does not park clients at off-screen
  coordinates. Bento uses reversible real geometry.
- Transfers prepare and validate the destination before removing the source.
- Constrained new windows enter the Bento solver or prepared Active ownership.
- Touch Card Line preserves a Bento composition as one logical group card. Its
  pane-visible live surfaces keep their native work-area positions and proportions
  inside one centered desktop view, including outer gutters, pane gaps, and dock
  clearance. Painting reconstructs each pane from the transferred normalized rect,
  so later live-frame acknowledgements cannot move it inside the group. The stored
  pane frame clips expanded decoration and shadow pixels out of workspace gutters.
  One translucent-black field replaces wallpaper behind the panes and all group
  paint state retires before an exact resume exposes native Bento again.
  Retained overflow stays owned and minimized. The group has no member paging,
  fan, or member carry; activation resumes the exact Bento session without a
  layout solve and restores neighboring ordinary cards from their own records.
  Ordinary Card Line cards retain their accepted presentation.
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

The current `main` production sources include the accepted ownership and
constrained-launch correction. The last confirmed installed Kadunce build used the
same production content. Revalidate source, package, control, and live provenance
before the next installation.

## Open limitations

- Pulling a member back from a stack, reorder intent zones, centered labels, and
  stack-position labeling still need product completion.
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
The Bento group-card candidate has focused full-work-area, pane-frame clipping and
repeated-projection geometry, translucent backdrop, session-contract,
no-member-paging, overflow-minimization, residue-free repeated exact resume,
first-snap refresh, and private two-output ownership lifecycle coverage but still
requires physical appearance review before promotion.
Physical review has accepted ownership, constrained launch routing, stack retention,
large-pane selection, monitor isolation, lifecycle, and the current live-rendering
model. Automated and private-compositor checks do not replace physical appearance,
frame pacing, hardware touch, fractional-scale, suspend, or live disable review.

## Safety

Run `bash tests/verify-control.sh` for every Kadunce change. After an authorized
installation, run `bash tests/verify-live-control.sh` in the graphical session and
confirm startup wiring. Do not infer a missing control from sandbox or D-Bus
transport failure. Do not stop the graphical session or toggle the effect without
explicit authorization.

Future work and ordering live only in `NEXT-ROADMAP.md`.
