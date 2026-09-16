# Shuffle Keyboard

## 1.0 Concept and Engineering Brief

- **Status:** Product concept approved for technical evaluation
- **Product:** Shuffle for Plasma
- **Publisher:** Good Input
- **Purpose:** Complete the premium touch experience by providing a first-party virtual keyboard designed specifically for desktop-class touch interaction.

## Product goal

Shuffle Keyboard should not be a conventional desktop keyboard shrunk onto glass, nor an attempt to recreate a phone keyboard.
The goal is:

> **A touch interface for keyboard-and-pointer desktop input.**

A Shuffle user should be able to detach all physical peripherals from a supported 10–13" device and continue operating Plasma without leaving the Shuffle experience.
The keyboard must therefore solve two related problems:

1. Fast, comfortable text input
2. Precision desktop interaction when touch alone is insufficient

The existing KDE/Wayland input stack should own text-input plumbing, locale handling and keymap correctness wherever practical. Shuffle should own the interaction design.

## 1. Base keyboard

The primary landscape keyboard uses four rows.
There is no permanent number row. 123 toggles the character area between letters and numbers/symbols.
Conceptual layout:
```text
 Q   W   E   R   T   Y   U   I   O   P

   A   S   D   F   G   H   J   K   L

 ⇧   Z   X   C   V   B   N   M   ,   .

 123   Ctrl   Alt          SPACE
```

**Priorities:**

- large touch targets
- generous spacing
- conventional QWERTY positioning
- Space reachable comfortably from either hand
- Shift available without consuming excessive width
- no permanent arrow cluster
- no permanent number row
- secondary desktop keys available without occupying primary typing space

The keyboard should favor accuracy and touch ergonomics over physical-keyboard fidelity.

## 2. Cascading action keys

The right side of the keyboard contains three progressively larger action targets integrated into the keyboard silhouette:
```text
                                ┌────────────┐
                                │ BACKSPACE  │  1×
                                └────────────┘

                              ┌────────────────┐
                              │     ENTER      │  1.5×
                              └────────────────┘

                           ┌──────────────────────┐
                           │       SHUFFLE        │  2×
                           └──────────────────────┘
```
These should visually cascade with the stagger and shape of the keyboard, rather than appearing as a separate toolbar.

**Relative sizing:**

**Backspace: 1×**

Large enough for reliable touch input, but intentionally not oversized.

**Enter: 1.5×**

A larger commit/action target.

**Shuffle: 2×**

The largest non-character control because it is the primary transition between typing and precision desktop interaction.
Backspace does not need to encourage rapid destructive deletion because Undo is immediately accessible through the edit surface.

## 3. Edit gesture surface

The cascading action keys create a small area of otherwise unused space.
That space becomes a dedicated editing gesture surface.
This is not a trackpad and should not behave like one. The user's hand should be able to move directly from typing into an editing gesture without entering a navigation mode.

**Current proposed gesture vocabulary:**

```text
2-finger tap       Copy
3-finger tap       Cut
Hold               Paste

Swipe left         Undo
Swipe right        Redo
```
The interaction should feel closer to pressing another keyboard key than operating a touchpad.

**Goals:**

- extremely short gestures
- generous recognition tolerance
- immediate feedback
- no pointer movement
- no scrolling
- no navigation behavior
- actions available without moving the hand far from typing position

The exact mappings remain prototype-dependent. Cut in particular should receive additional accidental-trigger testing because it is more destructive than Copy.

## 4. The Shuffle key

The Shuffle key replaces the generic idea of a PAD button.
It is the keyboard's transition between text input and precision desktop input.

**Two behaviors should be evaluated:**


### Hold Shuffle

Preferred default if technically reliable.
```text
Hold Shuffle
      ↓
keyboard region becomes trackpad
      ↓
perform pointer interaction
      ↓
release Shuffle
      ↓
keyboard immediately returns
```
This allows:

> **type → hold → point → release → continue typing**

with no persistent mode management.

### Tap Shuffle

Tap toggles/latches trackpad mode:
```text
Tap Shuffle
      ↓
Trackpad mode remains active
      ↓
Tap/exit
      ↓
Keyboard returns
```
This is useful when prolonged pointer interaction is required.
Both behaviors can coexist.

## 5. Trackpad mode

When Shuffle activates trackpad mode, the main keyboard area becomes the trackpad.
Do not squeeze a tiny permanent touchpad into the keyboard.
The full keyboard footprint provides a large precision surface suitable for desktop applications.

**Baseline behavior:**

```text
1-finger movement       Pointer
1-finger tap            Left click
1-finger hold/drag      Drag/select

2-finger movement       Scroll
2-finger tap            Right click
```
Additional trackpad gestures may be evaluated later.
However, existing Shuffle system gestures remain their own interaction system. Trackpad mode should not unnecessarily duplicate Workspace, Spread, Bento, Table or other system-level gestures already owned elsewhere by Shuffle.
The edit gesture surface remains conceptually separate from trackpad mode.

## 6. Navigation alternative

Some users may prefer conventional navigation controls instead of a software trackpad.
Shuffle should eventually allow a preference between:

- Trackpad mode
- Navigation controls

The navigation option could expose arrows, Home/End, Page Up/Down or similar desktop controls through the Shuffle key/layer.
This does not need to be the default.

## 7. Dynamic keyboard height

Keyboard size should not be limited to fixed Small / Medium / Large presets.
The upper boundary of the keyboard should be directly draggable.
```text
drag upward
    ↓
larger keyboard / larger targets

drag downward
    ↓
smaller keyboard / more application space
```
Shuffle should enforce sensible minimum and maximum sizes.
The user's selected height should persist.
Eventually, separate remembered heights may make sense for:

- landscape
- portrait

The goal is to let users balance screen real estate against their own touch accuracy.

## 8. Visual direction

MaiN Keyboard demonstrated a useful visual direction but is not the intended final implementation.

**Useful qualities from MaiN:**

- dark, understated presentation
- minimal chrome
- key shapes communicated partly through spacing and geometry
- subtle boundaries rather than conventional physical-keyboard boxes
- visually appropriate for a desktop environment

Shuffle Keyboard should use the existing Shuffle visual language, not create a separate keyboard aesthetic.

**Avoid:**

- skeuomorphic physical keys
- excessive outlines
- giant toolbars
- “gaming keyboard” styling
- unnecessary technical ornamentation

The cascading Backspace / Enter / Shuffle geometry should become part of the keyboard's recognizable silhouette.

## 9. MaiN prototype findings

MaiN was useful as a physical test but exposed several issues that Shuffle should explicitly avoid.

**Observed:**

- visually promising
- noticeable input latency when typing quickly
- dropped/lagging input at higher typing speed
- displayed Y/Z positions did not match actual resulting input
- permanent number row consumed valuable space
- Space placement favored left-hand use
- arrow keys consumed excessive permanent space
- Backspace target was undersized for touch
- standalone uinput implementation did not integrate as Plasma's managed virtual keyboard

These findings reinforce using native KDE/Plasma input-method plumbing where possible rather than inheriting MaiN's backend.

## 10. Engineering direction

Before implementation, audit the current KDE options, particularly:

- Plasma Keyboard
- Qt Virtual Keyboard integration
- KWin input-method plumbing
- Fcitx5 OSK where useful

**Preferred architecture:**

```text
KDE / KWin
owns input plumbing, locale and application compatibility

        ↓

Shuffle Keyboard
owns layout, interaction, sizing and product behavior
```
Do not build a new keyboard/input-method engine unless existing infrastructure proves insufficient.

**The initial implementation should prioritize compatibility with:**

- Qt/KDE applications
- GTK applications
- Chromium/Electron
- browsers
- terminals
- Shuffle Search
- Files
- lock-screen/session surfaces where technically possible

## 11. 1.0 boundaries

Shuffle Keyboard 1.0 does not need to become Gboard.

**Do not independently build:**

- autocorrect
- predictive typing
- swipe typing
- voice dictation
- custom dictionaries
- AI writing assistance
- a new multilingual IME
- emoji infrastructure

If mature system infrastructure provides some of these safely, it may be exposed later.
The 1.0 requirement is simpler:

> **Reliable text input and precision desktop control without a physical peripheral.**


## 12. Release blockers


**The following should be treated as hard failures:**

- **Dropped characters:** A keyboard that cannot keep up with normal typing is not releasable.
- Meaningful input latency
- Incorrect locale/keymap representation
- Focus loss caused by interacting with the keyboard
- Failure across major Qt/GTK/browser application classes
- Unreliable show/hide behavior
- Trackpad transition interfering with active text input
- Keyboard obscuring content without Shuffle adjusting usable workspace appropriately

## Product principle

Shuffle Keyboard completes a missing piece of the broader product promise:

> **A premium touch experience for Plasma.**

The user should not need a physical keyboard or mouse simply because an application still assumes desktop-class input.
Shuffle does not need to make every Linux application touch-native.

> **It needs to give the user's hands the tools necessary to operate them anyway.**

That is the distinction worth preserving.
