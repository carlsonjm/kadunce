# Current state

## Product

Kadunce is the open-source spatial-window component of Shuffle for Plasma. KWin
owns real windows and virtual desktops. Kadunce owns the touch interaction
model, card membership, output-local Bento sessions, input routing, and
compositor presentation. Table and Shuffle Keyboard are required
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
  individual card on the display that can hold one.
- A display presenting its layout answers every arrival with the layout, and
  never shows a window on top of live panes. A new window or a card the user
  calls forward grows the layout where it can, takes a pane where it cannot, and
  becomes an individual Active card only where no slot fits its minimum size, at
  which point the layout becomes a Spread group rather than staying behind it.
  The arrival claims the smallest slot its minimum size permits and only that
  slot's occupant leaves, as a nonselected card behind the layout; every other
  pane keeps its window, its size and its place. A side release still overrides
  that where the gesture exists, which no tablet gesture does. An arrival no slot
  can hold retires the layout into a Spread group before becoming a card, on both
  the launch and the activation path.
- `workspaceContext` reports the third presentation as `bento`, distinct from the
  Active card that would be drawn over it. It reported both as `active` before,
  which is the distinction two physical reviews turned on.
- Admitting a Bento group parks the restore record of an individual Active card
  the stage already presents instead of discarding it, so a window displaced
  into card ownership still returns where it began when Kadunce releases.
- A layout ends into card ownership. When it falls to one visible pane, that
  pane becomes an individual card carrying its pre-Bento record, and only
  explicit release or disable returns a managed window to the desktop. A pane
  carried to the top edge leaves the layout it is in and becomes the Active
  card, so on a display whose maximum is two panes an extraction ends the
  layout and leaves two individual cards. A display that cannot own cards has
  no destination for the rule and keeps its single pane. The gesture is
  physically accepted.
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
  Bento session without a layout solve. A pane whose own client changed its frame
  while the group was projected is placed back on the stored rect as the session
  resumes, rather than refusing the resume and leaving the group unopenable.
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
- The keyboard overlays the desktop. While Kadunce is loaded it declines
  KWin's own lift of the focused window through KWin's
  `OverlayVirtualKeyboardOnWindows` setting, held in memory and restored on
  unload, so showing, hiding or resizing the keyboard changes no card's size or
  place. When the keys would cover the Active card's text cursor, the card's
  frame stays where it is and its contents pan up inside it until the cursor
  sits a gutter above the keys; they roll further for a line typed below the
  keys or taller keys, never back down while the keyboard is up, and return
  exactly when it goes. A client that reports no cursor is not moved. Spread
  and Bento are untouched. A card the stage focuses by its own gesture keeps
  the keyboard down for the moment in which a client would have KWin raise it
  on focus. Physically accepted on 23 September.
- While Kadunce is on, the display that can own cards holds its windows as
  cards. Switching it on, at sign-in or from the tray, makes the window in use
  the Active card and every other window there a card, and a window that opens
  while that display holds no card starts it the same way. Physically accepted
  on 23 September.
- While Kadunce hosts the search launcher in Spread, a touch or click on the
  keys types into it rather than reading as a touch away from it; a touch
  outside both still closes it. Physically accepted on 23 September.
- A dialog is part of its application and never a card or a Bento pane. Over
  its own Active card or pane it floats at its own size and the layout is
  untouched. When its application is not in front, in Spread or behind another
  card, it waits hidden and unfocused while the application raises Plasma's
  attention flag, and picking the application brings it forward with the
  dialog on top. Switching Kadunce off gives waiting dialogs back. Physically
  accepted on 23 September.
- Cards and Bento layouts live on the virtual desktop where they started.
  Every other desktop is plain Plasma: windows open and move as ordinary
  windows, the bottom swipe and Spread are left to the desktop, and nothing
  there pulls the person back. Leaving cancels anything in hand, and coming
  back to a Spread left open finds the Active card. Physically accepted on 23
  September.
- A carry is not refused because some other window closed during it: a
  notification, tooltip or menu going away leaves an ordinary window's or a
  Bento pane's carry to the edge and its drop. The move trace names the check
  that refuses a pickup. Physically accepted on 23 September.
- A placement that does not settle is asked for once more, and a pane that still
  will not take its rect leaves for card ownership while the panes that settled
  keep theirs. A layout is never returned to the native desktop because a client
  would not take a rect; §13 keeps that for release and disable.
- On the tablet, Spread's bottom swipe starts at the bezel and nowhere else:
  the bottom 20 pixels, where every bezel swipe first registers. A swipe that
  starts on the dock, or on any layer surface that is not a panel, is left to
  that surface. Physically accepted on 22 September.
- A card and a Bento layout keep one gutter on every edge, the bottom included,
  and no layer surface is ever treated as an application or a card. Physically
  accepted on 22 September.
- An ordinary stack releases a member that is lifted and pulled up out of it,
  into the Spread where the stack stands, and rejoins one that never rose out.
  Sideways travel reorders instead, and the same upward gesture carried to the
  top edge is the edge action that makes the card Active.
- Minimizing a Bento pane hands it to card ownership as a sleeping individual
  card, and the layout keeps nothing it does not show. Waking it is an ordinary
  card waking and never returns it to Bento. Where no display can own a card
  there is nothing for the pane to become, so the layout keeps it instead. A
  layout that falls to one visible pane ends into card ownership, including
  when the pane it lost was the minimized one.

The current `main` production sources and installed candidate carry accepted
ownership behavior through Block 3: deliberate edge entry, no Bento overflow,
every arrival answered by the layout, a layout that ends into card ownership,
top-edge extraction, a refused snap that changes nothing, and the sleeping pane.
They also carry the constrained-launch correction and the physically accepted
Bento group-card presentation. Revalidate source, package, control, and live
provenance before the next installation.

The current local `main` adds physically accepted Spread application labels without changing
card geometry or input. Ordinary cards resolve their human application name from
desktop service metadata with window metadata and caption fallbacks, keep it
centered below the card, and put pageable stack position at the row's right edge.
A Bento group lists every visible pane application in pane order, including
duplicates, and omits stack position.

## Open limitations

- Resuming a projected Bento group leaves every card beside it owned by the card
  stage, which then presents Bento and keeps them hidden behind the panes rather
  than returning them to the desktop. The stage stops presenting only when the
  group was all it held. This is measured in source and by the isolated probes,
  not on an installed candidate.
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
- Card ownership is structurally single-desktop. `CARD-LIFECYCLE.md` gives
  every virtual desktop its own ownership session; the build gives one desktop
  cards and leaves the rest plain until Table, which is in 1.0, brings a card
  set per desktop. A card moved
  to another desktop with the window menu is not handled.
- Reorder intent zones still need product completion. A reorder commits at 82%
  of a card's pitch, 705px against an 860px pitch on the tablet's work area, and
  edge paging arms on a 300ms dwell inside a 115px edge zone. A sweep that long
  ends inside that zone, so a deliberate reorder and an accidental page are
  reached by the same travel.
- Some arrival and displaced-neighbor transitions remain visually incomplete.
- One of KWin's own internal windows is held as a card. It has no caption, is
  marked to skip the taskbar and switcher, and was measured on 22 September
  sized to the Active card. Which window it is has not been identified, and it
  is not excluded until it is, because a window Kadunce stops hiding is painted
  wherever it stands.
- Custom compositor motion does not yet fully follow platform animation scaling or
  reduced-motion preferences.
- The keyboard covers Bento panes, by decision: a pane keeps its rect and the
  person scrolls. It also covers ordinary windows that are not cards, and the
  Active card of a client that reports no cursor, such as Ghostty; none of
  them is moved.
- Panned contents jump rather than slide, and a tap in the strip just above a
  panned card reaches the hidden top of its client.
- A waiting dialog is not drawn on its card in Spread, and a dialog stays where
  it is when its card is carried. A Wayland dialog that names no parent is
  admitted as a card; none has been found in the apps surveyed. Nothing yet
  shows a waiting application: Ambient's row is Block 6 work, and whether the
  dock shows Plasma's attention flag is unmeasured.
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
drifted-pane resume, first-snap refresh, and private two-output ownership
lifecycle coverage. Physical
review accepted its geometry, tint, gutters, container-level rounded clipping,
exact resume, and repeated-entry behavior.
Physical review has accepted `CARD-LIFECYCLE.md` §8 on both branches, §5's
one-remaining-pane rule and §10's top edge, across four candidates on 20
September. The 21 September candidate added deliberate entry and pairing, a
refused side snap that leaves both the carried card's stack and the Spread
unchanged, a minimized pane leaving its layout as a sleeping card, and repeated
resume of a projected group after a pane's own client had moved it. A second
candidate the same day added a layout that survives a client which keeps moving
its own pane, and release of a stacked member by pulling it up out of the stack;
the compositor had that plugin mapped while the gestures ran. It has also
accepted ownership, constrained launch routing, stack retention, large-pane
selection, monitor isolation, lifecycle, and the current live-rendering model. Automated and private-compositor checks do not replace physical appearance,
frame pacing, hardware touch, fractional-scale, suspend, or live disable review.

The isolated nested-compositor probes under `tests/unload-probe/` are the
closest automated evidence to physical behavior. `ownership-transition`,
`side-runtime`, `sleeping-pane-runtime`, `settle-runtime`, `stack-runtime`,
`membership-runtime`, `launch-runtime`, `desktop-runtime` and `exit-runtime`
pass and assert the
accepted ownership contract: a launch joins a layout that can grow for it, one
no slot fits is refused awake and unowned, a full layout's displaced pane
becomes an awake individual card, a group resume leaves the cards beside it
owned rather than returning them to the desktop, a minimized pane leaves for a
sleeping card that waking does not put back, a placement a client keeps moving
out of costs that pane rather than the layout, and a stacked member is released
by rising out of the stack while one that never rose rejoins it. The
two probes that never passed were retired on 20 September: `column-runtime`
asserted a three-pane column on a display capped at two, which is the grammar
Block 3b holds dormant, and `tablet-entry-runtime` never reached an edge because
the harness could not start a carry from a press, a limit of the harness rather
than of the gesture. Neither retirement changes what the suite covers.

`verify-integrated-carry.sh` passes: sixteen nested-compositor sessions, the
candidate check, the source, package and control guards, and a read-only
session registration check, from a freshly captured source tree. It had not
passed since 19 September, and the reasons were stacked: each one hid the next,
because the matrix stops at its first failure. `tablet-runtime` asserted the `cardLine` presentation under the name
`spread`, which `Effect::workspaceContext` never reports, and `full-session` and
`lifetime-runtime` carried the same rewrite. `line-runtime` then failed at three
assertions of its own, all of them probe errors rather than defects, and all
three failed identically on `main`. Its release assertion asked for the held
card width exactly, which only a read that elapsed no time can see; the settle
itself was running and is measured doing so. Its two stack-seam assertions
inverted the front-first insertion depth and reused one carried identity across
two gestures, although the first release changes which card is the stack's
face. `line-runtime` now passes end to end.

Reaching past it exposed two more, both invisible for the same reason.
`tablet-desktop-runtime` crashed the compositor: the destination outline is
drawn for a Bento reservation or for a Card Stage entry, and only the first has
a reservation, but the detach label asked the empty one whether it detaches. A
native carry to a tablet side edge reaches that every time. The same session
then asserted the superseded entry contract --- a first side snap composing
Bento on the tablet rather than adopting it into cards --- and expected the
tablet to answer the dock band differently from an ordinary display, which it
no longer does now that both seams read capability rather than output identity.
Past those, `verify-bento-candidate.sh` was asking `/proc/<kwin>/maps` which
build the compositor had open. This kernel restricts ptrace to descendants, so
that file is unreadable even as the same user; the check now asks the effect,
which is the one process that can read its own map, and compares the inode it
reports with the candidate's.
Six failures in a row, one behind another, and only one of them a defect. A
matrix that stops at its first failure hides everything after it, so the cost of
leaving one member failing is not one check but every check behind it.
`active-admission-session.sh` runs and passes; it asserts Bento-to-Active
extraction over both selection paths, a refused extraction that leaves the
layout unchanged, repeated extraction, last-pane teardown, exact restore and
other-display isolation. It drives the controllers directly, and it now prepares
a real carry source and validates it the way the gesture does, rather than
supplying a stub that always agrees. That stub is why it passed for a week while
the gesture it stood for refused every time. No display the harness creates is an
internal panel, so no display there can own cards, and the gesture that reaches
the seam is still hardware-only: that half waits on physical review.

Installing a candidate does not by itself put it in the running compositor.
`install.sh` leaves the effect unloaded rather than reloaded, and KWin keeps the
previous plugin image mapped across an unload, so a freshly loaded effect can
still be the previous build. A compositor restart is what replaces the image,
and until one happens an installed candidate is unexercised.

The ownership observer has been live-verified on a restarted compositor
running the installed candidate. The retained Bento remainder it first reported
no longer exists in source and the rule that named it is retired, so the
observer now holds two rules. On the 21 September candidate it reported no
violation at any point, including deliberate entry, a refused side snap, group
resume, a minimized pane leaving its layout, and tray disable.

## Safety

`AGENTS.md` owns the safety requirements and the checks a change must run.
Current status: the mandatory control checks pass for the accepted sources and
again on the installed candidate in the graphical session, where the tray
enable/disable control released and restored ownership cleanly, including a
disable with a sleeping card owned, which returned that window to the desktop.

Ordering, tasks and open product decisions live only in `ROADMAP-CC.md`, and the
reasoning behind them in `ROADMAP-CONTEXT.md`.
