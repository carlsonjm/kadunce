# Post-freeze physical test findings — September 15, 2026

J reported these findings while testing the installed September 15 suite freeze.
This checklist replaces the earlier broad regression checklist above the planned
Missing Features section. A checked item means the reported behavior has been
reproduced and corrected on a later installed candidate; source-only validation
does not close it.

## Kadunce ownership and card manipulation

- [x] **Cross-display release target:** arrange windows on the monitor, enable
  Kadunce, drag one window to the tablet until Kadunce takes card ownership, then
  press Escape. Reported failure: the window returns to the monitor instead of
  releasing to the tablet where ownership was acquired.
- [x] **Initial Card Line ownership commit:** enter Card Line with previously
  unowned windows. Reported failure: some windows do not commit to card ownership
  until they are swiped through or clicked as Active. Visible proof: the floating
  dock becomes flat even though every window has Active sizing.
- [ ] **Bento launch routing on tablet:** with tablet-only Kadunce active, snap one
  window as Active, open Ghostty so it enters Bento, then launch Zen. Reported
  failure: Zen opens as an unmanaged normal window above Bento. Expected result:
  admit Zen into Bento's large pane when its minimum size requires that pane; if
  it cannot join the current Bento layout, give it Active sizing and minimize it
  as a prepared card ready to be called Active. It must not float unmanaged over
  the owned Bento workspace.
- [ ] **Bento projection into Card Line:** on a touch device, activating Card
  Line from Bento must retain the Bento-owned cards as one stack. The large-pane
  card is the default top/selected card. Returning to Bento must preserve the
  owned set and meaningful order. This projection must not change monitor Bento
  behavior because monitor outputs do not expose Card Line.
- [ ] **Remove a card from a stack:** pulling a card from a stack back into Card
  Line is too difficult.
- [ ] **Reorder within Card Line:** the useful drop strip between Active and its
  neighbor is too small and paging triggers before the intended reorder.
- [ ] **Card labels:** every card needs a centered label below it.
- [ ] **Stack position:** show stack position in the same row as the card label,
  aligned to the right edge.

Interaction direction for the manipulation problems: enlarge the invisible
intent zones without enlarging the visible rail, reserve a stable reorder zone
before paging becomes eligible, and use drag direction/history to distinguish
stack exit, neighbor insertion and paging. Any solution must preserve accepted
contact anchoring, current card geometry and direct edge paging.

## Ambient Tette

- [ ] **Constrained media packing:** on tablet, music currently drops only the
  duration at its preferred size. With multiple tasks, Ambient grows toward
  Temperance instead of shedding information. Expected constrained sequence:
  remove artist, then song, then previous/next controls; play/pause is the final
  compact media capability.
- [ ] **Concurrent transfer fit:** physical B1 test failed. Under task pressure,
  the centered icon dock still moves toward Temperance instead of remaining
  centered while Ambient sheds media details. During a download, media does not
  shed to reveal the filename.
- [ ] **Transfer interaction:** physical B1 test showed only the download icon;
  there was no useful filename and no visible completion checkmark. Pause/resume
  and false-success cases remain deferred until the geometry/lifecycle correction
  is actually visible.
- [ ] **Transfer completion handoff:** retain a brief truthful completion state in
  Tette. After the ongoing activity ends, Temperance may surface the completed
  transition when it remains useful or actionable. Do not duplicate an
  unactionable completion indefinitely on both sides.

## Temperance

- [ ] **Ticker regression:** ticker content appears only inside the arrow-control
  view box on both tablet and monitor.
- [ ] **Recent-notification layout:** recent notifications can merge into one
  visual box or fail to expand the popup to accommodate neighboring cards as
  grouped alerts do.
- [ ] **Label casing:** the visible label is still not lowercase; expected `do not disturb`.
- [ ] **Power icon scale:** power-control glyphs should be slightly smaller.
- [ ] **Popup clearance:** Temperance popup windows remain too close to the
  floating dock. Measure the settled external geometry; do not fake the gap with
  content padding.
- [ ] **Control Center and System Tray spacing:** align both surfaces with the
  accepted spacing hierarchy regardless of earlier edits.

## Tette drawers and shared controls

- [ ] **Close drawer:** use lowercase `close drawer`; it is a label/control with a
  pill outline and hover fill.
- [ ] **Explore Files reveal:** opening Explore Files should use the same drawer
  reveal language as the application drawer.
- [ ] **Application sort:** present sort as a label with a chevron.
- [ ] **Dropdown geometry:** every dropdown must remain inside its owning window;
  center list content, use a pill outline for the active value, and hover fill for
  available choices.
- [ ] **Files visual alignment:** audit remaining Files spacing, hierarchy,
  highlights, controls, casing and rounded continuity against the design kit.

## Stop conditions

- Any missing window, stuck input, or broken Kadunce disable control blocks the
  release immediately.
- Preserve Tette Dot, Temperance Bell, Weather, tray, Speaker, performance
  selector, Kadunce identity, provider icons and proper names.
- Do not reopen unrelated historical reports while correcting this checklist.
