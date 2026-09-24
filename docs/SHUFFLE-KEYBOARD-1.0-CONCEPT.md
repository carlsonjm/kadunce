# Shuffle Keyboard product contract

**Status:** Required for Shuffle 1.0. The technical foundation is chosen
(`shuffle-keyboard` `docs/FEASIBILITY.md`), and the build is being rebuilt to the
22 September direction.

Shuffle Keyboard provides reliable text input and precision desktop control on a
supported 10–13 inch touch device without physical peripherals. KDE/KWin or another
mature system input-method stack owns delivery, locale, keymaps, and application
compatibility. Shuffle owns layout, interaction, sizing, and presentation.

## Base keyboard

- Four landscape rows with large, generously spaced touch targets.
- Conventional QWERTY positioning without a permanent number row or arrow cluster.
- `123` switches the character area to numbers and symbols.
- Space is reachable from either hand; Shift, Ctrl, and Alt remain available without
  crowding primary text input.
- Secondary desktop keys may use a layer rather than permanent width.

The right edge contains two cascading targets integrated into the silhouette:

- Backspace: 1×.
- Enter: 1.5×.

A third 2× Shuffle target and the mode transition it carried are retired; see
`Superseded by use`.

## Scrub columns

The keyboard respects Kadunce's 10 px gutter, so its key block does not reach the
work area's edges. The width either side is not padding: each side carries a scrub
column, a vertical control that is close to invisible until a finger arrives. Both
were approved on 22 September.

- **Left column: history.** A multi-step undo and redo scrub, not a single undo.
  Down is back in time and up is forward. Each notch is one step. Because the
  document shows what the scrub will remove while the finger is still down, and
  sliding back before release restores it, the gesture carries its own cancel. A
  tap cannot do that, which is why tapped undo was dropped.
- **Right column: key height.** Key width never changes, so the gutter, the key
  columns and the space bar keep their positions at every setting; only key height
  does. The keys lie over the workspace rather than reserving it (`DECISIONS.md`
  § The keyboard overlays), so a shorter keyboard uncovers more of the card
  beneath it, and the user is trading accuracy for screen rather than picking a
  key size. A
  bright notch marks the default, which is a square key.

  This is the sharpest conflict with the built candidate, which derives the typing
  block's width from the selected row height so that ordinary keys stay square.
  Under this direction a key is square only at the default notch, and away from it
  the key stops being square rather than the keyboard changing width. Whoever
  builds this changes that derivation first; everything else in the column follows
  from it.

Neither column is on the top edge. Show and hide owns that edge alone.

The device has no haptics, so a notch is felt two other ways: it holds briefly
before it gives, decoupling the finger from the indicator the way a detent does,
and it marks itself with a short click through the touch sounds the build already
has. Whether the hold reads as quality or as lag is a hardware judgement and is
not settled here.

Copy, cut and paste return to ordinary modifier chords. They were gestures only
because the surface that carried them existed; nothing is lost that the `ctrl` key
does not already do more predictably.

## Precision surface

The precision surface is still the full keyboard footprint acting as pointer and
scroll input. What changed on 22 September is how it is entered, and that it no
longer replaces anything.

**The space bar is the pointer.** Press it and slide: the finger that holds is the
finger that points, so there is no second hand and no key pinned in a corner. The
keys step back visually but stay exactly where they are, so there is no mode to
escape and no visual swap between typing and pointing.

Arming is by distance, not time. Sliding past roughly ten pixels turns the touch
into a pointer; releasing without sliding types a space. No timer sits between the
user and the most-pressed key on the keyboard. Once armed, the gesture keeps
tracking beyond the space bar's own bounds, so travel is not capped by the key.

A control at the space bar's right end latches the surface for longer work, which is
the same momentary-and-latched pair the retired Shuffle key carried, moved to where
the thumb already rests. Pointer acceleration around 2.4x is an accepted baseline,
not a tuned value.

Baseline precision behavior:

| Gesture | Action |
| --- | --- |
| One-finger move | Pointer movement |
| One-finger tap | Primary click |
| One-finger hold/drag | Drag or selection |
| Two-finger move | Scroll |
| Two-finger tap | Secondary click |

The precision surface does not duplicate system-level Shuffle gestures. Editing
gestures remain a separate recognizer and state.

## Dynamic height

Height is set by the right scrub column, in notches, with the default marked. The
chosen height persists, and landscape and portrait may eventually remember separate
values. The keys reserve no workspace, so a height change moves only how much of
the card they cover, and the Active card's contents re-pan to keep the cursor in
view.

A continuously draggable upper edge is retired; see `Superseded by use`.

## Visual contract

Use the shared Shuffle visual language: dark, restrained, minimal chrome, clear
spacing, and subtle boundaries. Avoid skeuomorphic keys, dense outlines, permanent
toolbars, and ornamental technical styling. The cascading action geometry is part of
the recognizable keyboard silhouette.

### Geometry against the work area

Approved 22 September. On the tablet's 1443x894 work area the key block sits at 80%
of the width at its default position, measured on the build rather than derived. The
keyboard takes Kadunce's 10 px gutter on every side, so it aligns with an Active card
and with any window edge, and the keys are sized up about a tenth from that
measurement. The two scrub columns consume what is left, so no region of the
keyboard is empty and there are no bars.

The keyboard keeps a hard rounded edge. A density falloff like the Bottom Surface's
was tried and rejected on sight the same day: surfaces that meet the screen edge
fade, and objects that float in the layout keep their edge, which is what makes the
keyboard read as a sibling of the Active card rather than of the dock. The falloff
is pinned for a possible revisit, and the gutter plus the scrub columns may already
have solved what it was for. Two masked axes also pinch the corner, because
multiplying two linear ramps collapses density toward the corner faster than along
either edge; a square corner would need one distance-based mask.

### Accepted visual direction

- Retain MaiN Keyboard's open spacing, floating labels, and quiet lower key edges.
- Use the shared dark surfaces, Ghost White labels, rounded controls, restrained
  borders, and accent only for meaningful state.
- The built candidate's arrangement governs, because J has typed on it and
  accepted it. `123` and Tab take the
  first two left edges and Shift the third, where a tap is one-shot Shift and a
  double tap locks Caps. Backspace and Enter share the right edge. The bottom row
  is `ctrl`, `alt`, a large Space, and the Tette Dot Meta key, which is the
  protected resting brand mark rather than a text label.
- Backspace is one ordinary-key width. Enter is approximately 1.5 key widths.
- Space is the widest control on the keyboard and is wider still now that it also
  carries the pointer. Sizing the keyboard up serves that directly, because a wider
  space bar is more travel before the gesture has to track beyond it.
- There is no Shuffle surface key and no gesture hint at the lower-right corner.
- Shift carries both one-shot Shift and Caps lock; there is no separate Caps key.

## Engineering constraints

- Build on the chosen foundation: a Shuffle front end over Qt Virtual Keyboard,
  Plasma Keyboard's input-method client and KWin's text-input delivery.
- Do not build a custom input engine unless mature system infrastructure cannot meet
  the release contract.
- Verify Qt/KDE, GTK, browsers, Chromium/Electron, and terminals.
- Treat dropped characters, wrong keymaps, focus loss, meaningful latency, unreliable
  show/hide, or a keyboard that moves or resizes a card as blockers.
- Keep pointer, editing, resize, keyboard, and system gestures in explicit,
  non-overlapping ownership states.
- Lock-screen/session surfaces are supported only where the system API permits safe
  integration.

## 1.0 boundaries

Shuffle 1.0 does not independently implement autocorrect, prediction, swipe typing,
dictation, custom dictionaries, AI writing, a multilingual IME, or emoji
infrastructure. Mature system-provided capability may be integrated when it does not
compromise input reliability.

## Superseded by use

J built and used the implementation before this contract was tested, and four of its
hypotheses did not survive. They are recorded so they are not rebuilt.

- **The 2x Shuffle key and hold-for-precision.** Pointing was modal: holding the key
  replaced the keyboard, so every switch between typing and pointing cost a visual
  swap and a finger held in a corner while the other hand pointed. The key also took
  two rows of the right edge, where enter, backspace and punctuation want to be.
  Space-bar pointing replaces both halves.
- **The 10% side touch pads.** These were the field fix for the above, and they
  fixed the mode problem while breaking travel: about 143 px mapped against 1443 px
  of screen means repeated stroking, which is where hold-to-move and
  release-to-click becomes unpleasant. Separating move from click is what the build
  does today and it is not natural either.
- **The tapped edit vocabulary.** Only undo and redo earned their place, and they
  are better as a scrub than as a tap. Copy, cut and paste return to modifier
  chords.
- **The continuously draggable upper edge.** The default size is already the tested
  ideal and a continuous drag makes it hard to return to, so the control loses the
  one setting known to be right. It also put resize and show-and-hide on the same
  edge, which is two meanings on one drag rather than a threshold to tune.

The pattern across all four is worth keeping: each failure came from two functions
sharing one affordance, or from a mode the user had to leave. Neither is present in
what replaced them.

## Acceptance

A supported touch device can type quickly without loss or incorrect mapping, change
keyboard height, enter and leave the precision surface without focus loss, perform
pointer and editing actions reliably, and return immediately to text input.

Additionally, for the 22 September direction: an ordinary space is never lost to the
pointer gesture, a history scrub can be cancelled inside the gesture, and the notch
hold reads as feedback rather than as delay on the device itself.
