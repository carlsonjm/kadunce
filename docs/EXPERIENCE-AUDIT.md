# Product and experience audit, 23 September

A read-only review of the whole suite against what it is trying to do for the
person holding the tablet. It covered the built product, its contracts and the
plan, and it asked where Shuffle has not yet solved something and where it has
got in its own way while satisfying another rule.

Every finding is numbered and names the `ROADMAP-CC.md` line that answers it. A
roadmap line tagged `(audit N)` closes finding N when it is ticked. This document
records what was found. It is not updated as the work proceeds; the roadmap
carries the progress and `ROADMAP-CONTEXT.md` the reasoning.

## Rulings J made on the audit

- The dock steps aside for the keys rather than staying under them, because a
  dock under the keys is worse for typing. `DECISIONS.md` § The dock steps
  aside for the keys.
- The tray's on/off switch has to be reachable on every build that is tested or
  shipped, not on screen at every moment. `DECISIONS.md` § The tray control is
  mandatory.
- Table is tentatively 1.1, depending on how long integrating the core concepts
  takes. Open decision 5.
- Shuffle Lock ships in the consumer bundle whatever else 1.0 holds. Open
  decision 2.

## Release-level gaps a person meets first

1. **Search can't be typed into by touch.** A touch on the Keyboard reads as a
   touch outside the search launcher, which closes. Tettegouche's roadmap calls
   it a release matter, and the suite plan did not carry it. Block 13, first
   line.
2. **Dialogs aren't kept with their app.** The card contract says a dialog
   follows its app. Nothing in the build does that, and no test opens a dialog.
   By reading, a save or confirmation dialog can become its own card or take a
   Bento pane. Block 13: open dialogs on the tablet, then keep each with its app.
3. **A window hidden from the task switcher is held as a card.** The
   unidentified system window recorded in `CURRENT_STATE.md` is one. The
   contract already excludes such windows, so following it is the fix. Block 13.
4. **The first text field of a session flashed and didn't raise the keys.**
   Block 13.
5. **A text box tapped right after choosing a card can have its keys put back
   down,** because the rule that keeps the keys down after the stage focuses a
   card lasts a second. Block 13.
6. **The Claude window's first drag off the desktop is refused** and falls back
   to an ordinary move; the second drag works. True before the keyboard work.
   Block 13.
7. **The on/off switch leaves the screen while the keys are up.** Closed by J's
   ruling above; no task.

## The contract promises more than the structure holds

8. **Cards live on one screen, chosen by its name.** The build has one set of
   cards, on the output named like a laptop's built-in panel. A touch monitor on
   a desktop computer gets none, and a laptop with a non-touch panel gets them
   there rather than on its touch monitor. Block 14: measure, then put cards on
   the screen the touchscreen drives.
9. **Nothing answers a virtual-desktop switch.** The contract gives each desktop
   its own session. Block 14: measure, then give the switch the answer J rules.
10. **The contracts disagree about the monitor.** The product contract says a
    monitor never presents cards; the card contract lists cards and stacks on
    every display. The monitor still fills itself with whatever windows fit,
    the rule the tablet rejected as the solver deciding for the person, and a
    window it cannot show moves onto the tablet. Block 14: options for the
    monitor, and contracts that say what is built.
11. **Table is architecture, not a feasibility check.** Entering a destination
    Spread needs a card set per virtual desktop. Open decision 4; Block 8 waits
    on Block 14.
12. **1.0 scope.** Table, Lock and the approved Keyboard were all at zero. Ruled:
    Table tentatively 1.1, Lock in the bundle. Open decision 5; Block 9b.

## Bandaids in the plan, and where they were steered

13. **The Keyboard's arrival motion was covering a hand-off.** The dock slides
    away, gives up its room, and the keys are nudged down into it; motion hides
    that rather than removing it. The 22 September direction also puts a gutter
    at the Keyboard's sides, where the dock leaving will show. J ruled the dock
    steps aside; the motion now follows the rebuild. Block 9 rebuild, then
    Block 5 arrival motion.
14. **The keyboard overlay removed premises the plan still assumed.** Three of
    the four items in 12a, the Keyboard concept's promise that a window grows by
    what the keys give back, Tettegouche's item written for a keyboard that
    reserves space, and the ask for a gap above the keys so they read as pushing.
    12a re-cut; Block 9's concept line; Block 6's first line; the gap above the
    keys dropped unless J wants it for its look.
15. **Block 4's reordering could never close.** Its own notes say the Spread
    deck dissolves it, and the deck waits on three blocks. Moved to Block 7b.
16. **The tablet's missing control was shelved behind a monitor-only one.**
    Displacing by side needs a side gesture the tablet lacks; letting a drop
    onto the Bento picture name the pane it replaces is what the tablet needs.
    Block 4, reordered; still shelved until MVP criteria are met.
17. **Timers that guess intent.** The one-second hold on the keys, the handle's
    300ms fallback, and download rows that disappear after a quiet period. Each
    works most of the time and fails without explanation. Block 13 for the keys;
    Block 6b's timer audit for the rest.
18. **The dock took over the handle's job.** Twenty-one of twenty-eight pulls
    missed the six-pixel handle, so every dock icon now answers tap, long press,
    sideways drag and pull up. Accepted; watched, with no task.
19. **Covered text was removed, not solved.** A terminal's prompt, and any
    message box pinned to the bottom of its window, stay covered, and scrolling
    does not move them. Block 15.
20. **Temperance's width has three fixes on one problem.** It measures itself,
    reads the dock's published width, and is clamped by the dock anyway. Block
    5: once the flanks hold, retire the fixes they made redundant.
21. **Motion was split five ways.** Arrival and displacement, the Keyboard's
    entrance, the Active card, the pan and the deck each set their own timings.
    Block 16.

## What a touch environment should do that Shuffle does not yet

These are design conversations with J before any code. J named 22 to 24 as the
place to start.

22. **Show the result before release, and say when a gesture is refused.** A
    side snap's partner comes from an order nobody sees, the upper or lower half
    of an edge silently picks the pane size, the tablet cannot show which pane
    an arrival displaces, and a refused drag does nothing. Block 15.
23. **Start in cards and come back as left.** After every sign-in, windows sit
    on the desktop until the first deliberate action, and stacks, pairs and
    order are gone after a sign-out, restart or reload. Block 15.
24. **The keys follow the text.** Tap a text box and the keys come; tap away and
    they go. The first-field flash, the one-second hold, clients with no cursor
    and the closing launcher all break it. Block 13 (1, 4, 5); Block 9's
    cross-application pass.
25. **Close an app by touch.** Spread has no gesture for it; upward travel there
    already means leaving a stack or becoming Active. Block 15.
26. **The top edge.** Pulling down returns to the current card, which a tap in
    Spread already does; people expect notifications and quick settings there.
    Block 15.
27. **Posture.** The model is chosen by screen, while the tablet becomes a laptop
    when its keyboard is attached. Block 15.
28. **Pointing without the keys.** The trackpad lives in the Keyboard, so a
    right-click costs raising half a screen of keys. Block 15.
29. **Copy and paste by touch.** The concept returned them to Ctrl chords, which
    nobody makes on glass. Block 9.
30. **Portrait, and sleep and wake,** have no physical pass on record. Block 15.
31. **One home.** Apps are found in the dock, in Spread, in the search launcher
    and in Ambient. Block 15.

## Smaller loose threads

32. **The notification ticker pages only on hover,** so a finger cannot page it.
    Block 7.
33. **Log Out is one tap with no confirmation.** Block 7.
34. **Ambient's controls are small,** 20px icons in a 42px strip whose own tap
    opens details, and Files opens on a double tap. Block 6.
35. **Reduced motion and reduced transparency** have no implementation. Block 16
    and Block 5.
36. **A pull on the handle makes the keys flicker once,** within about four
    milliseconds. Judged with the arrival motion, Block 5.
37. **Panned contents jump rather than slide** (Block 16), and a tap in the
    strip above a panned card reaches the app's hidden top (12a).
38. **The drop refusal on the withdrawn 23 September candidate** was never
    reproduced. The move trace now names the refusing step; watched, with no
    task.

One item raised in the audit's first telling is not a defect: a resumed Bento
group keeping the cards beside it owned and hidden is what `CARD-LIFECYCLE.md` §6
asks for.

## Stale documents

39. Block 6b carries correcting these. They were found stale on 23 September.

- **Kadunce:**
  - `KNOWN-ISSUES.md` still calls stack extraction incomplete and the Bento
    group's physical review open.
  - `PRODUCT-CONTRACT.md` § Input and `CARD-LIFECYCLE.md` §15 use "page line"
    and "Solo Card", which `TERMINOLOGY.md` does not define.
  - `CURRENT_STATE.md` and `KADUNCE-TABLE-1.1-CONCEPT.md` call Table required
    for 1.0, and `CURRENT_STATE.md` says the Keyboard has no accepted
    implementation.
  - `SHUFFLE-KEYBOARD-1.0-CONCEPT.md` says its technical evaluation is queued.
  - `ROADMAP-CONTEXT.md` Block 5 still says the dock extent has no consumer, the
    dock geometry does not exist yet, the material's double paint has no answer,
    and that J accepted a pull costing the app its focus, which the pull
    decision replaced.
  - Two code comments: `DesktopStageController::handleWindowAdded` names
    activation recency, which was removed, and `Effect::surfaceOwnsTouchAt`
    says a swipe from the dock is the bottom swipe, which the bezel rule
    replaced.
- **Downstream (`shuffle/`):**
  - The Bottom Surface contract says the switch is present at all times and
    that the handle reserves nothing; the handle reserves six pixels.
  - Its status lines, `CLAUDE.md` and `README.md` still describe a repository
    holding only the boundary.
  - The material document's handle specification no longer matches the build.
- **Keyboard (`shuffle-keyboard/`):**
  - `README.md` says the handle reserves nothing, and `CLAUDE.md` still waits
    for the Bottom Surface.
  - `docs/PHYSICAL_ACCEPTANCE.md` expects a text field to raise the keys every
    time, the dock sliding down to be visible, and a tap to land on the bar
    itself. The dock strip now also raises the keys.
- **Tettegouche and Temperance:**
  - Their docs put Temperance on the left and Ambient on the right; the dock
    puts them the other way round.
  - Temperance says it reads the dock's published width, and the suite context
    says nothing does.
  - Temperance's `SWARM.md` handoff describes a fix already made, and one of
    its known limitations is an acceptance.
  - Temperance's README keeps the clock a separate widget, which Block 5
    reverses.
