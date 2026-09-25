# Shuffle execution plan

The checklist for the whole suite, in the order the work is done. One line per
task: `[x]` done, `[ ]` open. Why a task exists, what was measured and what it
needs to know live in `ROADMAP-CONTEXT.md` under the same block heading.

## Current target: personal daily driver

Shuffle becomes J's own daily system first. Work outside this list waits.

**Missing capability**

- Table (Block 8).

**Verification**

- Keyboard acceptance (Block 9).

**Daily-use fixes**

- The keys on the first tap (12f).
- Tettegouche with the Keyboard up (Block 6).
- Manipulation defects met in daily use (Block 4).

## Not current

- Product packaging (Blocks 10, 10b).
- Spread Deck (Block 7b).
- Advanced monitor layouts (12e).
- Public site (Block 11).
- Post-1.0 polish (Blocks 16, 12g).
- Also waiting: the rest of Block 6, and Blocks 13, 15, 7, 6b and 12a–d.

## Blocks

Open blocks are listed in priority order. A block's number is its name, not its
place in the order. A line tagged `(audit N)` answers finding N in
`EXPERIENCE-AUDIT.md`.

| Block | | Status |
| --- | --- | --- |
| 1 | Refactor enablement | Done |
| 2 | Ownership foundation | Done |
| 3 | Ownership behavior | Done |
| 3b | Bento layout grammar | Done |
| 10a | Integration repository | Done |
| 14 | Ownership scope | Done |
| 5 | Bottom Surface | Done |
| 9b | Shuffle Lock | Done |
| 9 | Shuffle Keyboard | Current: acceptance |
| 8 | Table | Current: runtime proofs, then a card set per desktop |
| 6 | Tettegouche completion | Current: the Keyboard line only |
| 4 | Kadunce manipulation | Current: defects met in daily use |
| 12 | Post-MVP debug | 12f current; the rest waits |
| 13 | What a person meets first | Not current |
| 15 | Consumer fundamentals | Not current |
| 16 | Motion | Not current |
| 7 | Temperance event boundary | Not current |
| 6b | Maintainability and lifecycle audit | Not current |
| 7b | Spread deck motion | Not current |
| 10 | Downstream assembly and installation | Not current |
| 10b | Package and interface identity | Not current |
| 11 | Host and public site | Not current |

## Block 1 — Refactor enablement

Done. A cold boot reaches the direct edge router, an installed build is the one
running, the checks test behavior rather than code shape, and the terminology
contract is applied.

## Block 2 — Ownership foundation

Done, and verified on a restarted compositor.

## Block 3 — Ownership behavior

Done. Deliberate edge entry, no Bento overflow, every arrival answered by the
layout, a layout that ends into card ownership, top-edge extraction, and a
refused snap that changes nothing.

## Block 3b — Bento layout grammar

Done. One pane cap per display, orientation from the work area, and side contact
choosing the shape.

## Block 10a — Integration repository

Done, 21 September.

## Block 14 — Ownership scope

Done, 24 September. Cards stay on the desktop they started on and follow the
touchscreen, a display coming or going keeps them there, and the monitor
organizes what it shows and sends what has no room to the dock. The tray is
reachable with a monitor plugged in. Smarter monitor layouts are 12e.

## Block 5 — Bottom Surface

Done, 25 September. The Bottom Surface is a Plasma panel Shuffle owns: a centred
dock with everything Plasma's task manager does, its long-press sheet with live
pictures, the Keyboard's handle and arrival, and Temperance's clock, calendar and
linked events beside it. The band is one eased fade whose black rises like a tide
for a window; Plasma's panel Opacity chooses Auto, Clear or Blackout; and the band
is the panel's height, 64 px by default, with the dock sized from it.

## Block 9b — Shuffle Lock

Done, 25 September. Shuffle Lock is what KDE's screen locker shows; the locker
alone decides. It is a lock-only Plasma shell chosen for the locker through
KWin's environment, so the desktop keeps KDE's shell: piano black, Temperance's
clock as one block, an outlined credential box whose rim lights red or green,
Sleep and a two-tap Shut down, the person's name, and the Keyboard's handle. The
sign-in screen keeps KDE's look and has the Shuffle Keyboard, so the tablet signs
back in without peripherals. See `../shuffle/docs/LOCK-CONTRACT.md`.

## Block 9 — Shuffle Keyboard

- [x] Audit Plasma Keyboard, Qt Virtual Keyboard, KWin input methods and Fcitx5.
- [x] Rebuild slice 1: the card, fixed key width, height limits. (audit 13)
- [x] Rebuild slice 2: the history and height scrub columns.
- [x] Rebuild slice 3: the latched trackpad.
- [x] Move the Keyboard's own grab and resize onto the dock's handle; height
      goes to the right-edge scrub column.
- [x] Bring J how the keys are put away: by the handle.
- [x] Bring the concept's height and reservation lines in line with the
      Active card making room for the keys. (audit 14)
- [ ] Prove the four-row layout, controls, height, and the switch to the
      precision surface.
- [ ] Verify locales, keymaps, focus, latency and loss-free input across Qt/KDE,
      GTK, browsers, Electron and terminals. (audit 24)
- [ ] Bring J options for copy and paste by touch. (audit 29)
- [x] The Active card makes room for the keys, and a pull never takes the
      focus. (audit 19)
- [ ] Keep autocorrect, prediction, swipe typing and dictation out of 1.0.

**Done when:** a 10 to 13 inch touch device types and points reliably with no
physical peripherals.

## Block 8 — Table

- [x] Choose Table's interaction and look: tabs scrubbed from the top edge.
- [x] Audit KWin and Plasma virtual-desktop APIs and lifecycle, from the 6.7.5
      source.
- [ ] Prove on a private prototype in the running session: a desktop that is not
      current painted at full size on every display, with its cost and
      freshness; a switch without the slide; and creating, naming, moving a
      window to and removing a desktop, each confirmed through KDE's D-Bus.
- [ ] Give every virtual desktop its own cards and layouts, replacing the
      one-desktop rule. (audit 9)
- [ ] Follow KDE's own changes: a window moved by the window menu or a rule, a
      desktop removed or reordered, a window opening on a desktop not in view.
- [ ] Prove multi-display behavior without changing Bento.
- [ ] Bring J how Shuffle keeps per-display desktop switching off, which Table
      assumes (J, 25 September).
- [ ] Bring J options for KDE's own switching beside Table: its three-finger
      touchscreen swipe once two desktops exist, and its desktop-name pop-up.
- [ ] Keep Table's workspace rules in Kadunce: a new desktop named after its
      first card's application, an empty one dissolved unless pinned, pins
      stored by desktop id.
- [ ] Build the smallest complete prototype and test it physically: the scrub,
      the depth line, a card carried to a tab and to `+`, and cancel.
- [ ] Implement the accepted interaction.
- [ ] Give the dock's sheet a way to move a window to another desktop.
- [ ] Choose the shortcut that opens Table.
- [ ] Bring J the option of a monitor layout's overflow going to another desktop
      through Table instead of the dock.

**Done when:** a window moves between real KDE desktops through Table without
breaking normal desktop switching.

## Block 6 — Tettegouche completion

- [ ] Search and Files stay usable with the keys covering half the screen.
      (audit 14)
- [ ] Ambient release validation against `AMBIENT-CONTRACT.md`.
- [ ] Show an app whose dialog is waiting as an Ambient row; a tap brings the
      app forward with its dialog. Decide whether an app's own attention
      request counts too.
- [ ] Bring J options for showing, in Ambient, windows a monitor layout sent to
      the dock for lack of room.
- [ ] Media metadata priority and dock-aware width.
- [ ] Bring J touch-first Ambient and Files: control size, and one tap to open.
      (audit 34)
- [ ] Transfers: pause, completion, and conflict choices that never overwrite
      silently.
- [ ] Files: properties, previews, recursive search, Recent, error states.
- [ ] Storage lifecycle without weakening KIO ownership.

**Done when:** each slice has automated checks and an installed acceptance pass.

## Block 4 — Kadunce manipulation

- [x] A resumed layout survives a pane that will not take its rect back.
- [x] Make stack extraction reliable.
- [x] Normalize live terminology to Spread.
- [ ] Let a Spread drop onto the Bento group name the pane it replaces.
      (audit 16)
- [ ] First Card or Bento entry adopts every eligible window on that display and
      desktop at once.
- [ ] Displace by side, on a display that has the side gesture.
      (audit 16)

**Done when:** manipulation passes physical review on supported hardware.

## Block 13 — What a person meets first

- [x] Type a whole search by touch without the launcher closing, in
      Tettegouche and in Kadunce's search guest. (audit 1)
- [x] Kadunce starts in cards when it is switched on, at sign-in and from the
      tray. (audit 23)
- [x] Name the check that refuses a pickup, as a refused drop is named.
      (audit 6)
- [x] Open dialogs on the tablet in Active, Spread and Bento, and record what
      each does. (audit 2)
- [x] Keep a dialog with its app: never its own card, never a Bento pane.
      (audit 2)
- [ ] Draw a waiting dialog on its card in Spread.
- [ ] A dialog travels with its card when the card is carried.
- [ ] Never hold a window hidden from the task switcher as a card. (audit 3)
- [x] The keys never come up because an app focused its own field.
- [x] The first drag of a window after Kadunce is switched on adopts it, as the
      second does. (audit 6)

**Done when:** on the tablet, a person searches and saves a file, and nothing
goes wrong.

## Block 15 — Consumer fundamentals

Each line brings J options first. An accepted answer becomes a task in the block
that owns it.

- [ ] A gesture shows its result before release, and a refused one says so.
      (audit 22)
- [ ] Come back the way it was left after sign-out or restart. (audit 23)
- [ ] Close an app by touch from Spread. (audit 25)
- [ ] Type into text the keys cover in a Bento pane or an ordinary window.
      (audit 19)
- [ ] What a pull down from the top edge brings. (audit 26)
- [ ] Whether attaching the keyboard changes the mode. (audit 27)
- [ ] Right-click and pointing without raising the keys. (audit 28)
- [ ] One place to find everything. (audit 31)
- [ ] One Shuffle settings app for what each component keeps in its widget's
      settings.
- [ ] Check portrait, and sleep and wake, on the tablet. (audit 30)

**Done when:** every line has J's ruling, and every accepted answer is a task in
its block.

## Block 16 — Motion

- [ ] Complete arrival, displacement, cancellation and neighbor motion.
      (audit 21)
- [ ] Make compositor motion follow animation scaling and reduced motion.
      (audit 35)
- [ ] Review the Keyboard's arrival, the dock stepping aside and card motion
      together. (audit 21)

**Done when:** every custom motion in the suite uses the shared timings and
honours reduced motion.

## Block 7 — Temperance event boundary

- [ ] Add only authoritative, useful system events, deduplicated and dismissable.
- [ ] Keep live progress and actions with Tettegouche.
- [ ] Page the ticker with a finger, not only a hover. (audit 32)
- [ ] Bring J whether Log Out asks before acting. (audit 33)
- [ ] Revisit presenter switching once its lifecycle is proven.
- [x] Refine action-pill typography and padding.

**Done when:** events are authoritative and never duplicated.

## Block 6b — Maintainability and lifecycle audit

- [ ] Remove duplication that creates reliability or maintenance risk.
- [ ] Verify Tettegouche and Temperance activity ownership and cleanup.
- [ ] Audit timers, model lifetime, responsive calculations and resource paths.
      (audit 17)
- [ ] Correct the stale documents the audit lists. (audit 39)
- [ ] Confirm the protected custom icons are unchanged.

**Done when:** the three components are lifecycle-safe with no duplicated state.

## Block 7b — Spread deck motion

- [ ] The row follows the finger, coasts and snaps to the nearest card.
- [ ] Rebuild browsing on that motion.
- [ ] Decide the row's shape.
- [ ] Retire edge-dwell paging once a throw reaches a distant card.
- [ ] Reorder with the deck rather than a third distance.
      (audit 15)
- [ ] Draw a held card from under the finger.
- [ ] Give the row something that shows its order.

**Done when:** dragging and throwing the deck settles where the hand expects, and
reordering and paging are never confused.

## Block 10 — Downstream assembly and installation

- [ ] Consume pinned component versions with provenance.
- [ ] One Fish-safe installer with dependency detection.
- [ ] Versioning, upgrade, rollback and uninstall, with the tray disable control
      working throughout.
- [ ] Fresh-machine installation test.
- [ ] Ship the KWin plugin as a package (Open decision 1).

**Done when:** a fresh machine installs, updates, rolls back and uninstalls
Shuffle with no repository knowledge.

## Block 10b — Package and interface identity

- [ ] Choose the suite's one reverse-DNS namespace.
- [ ] Rename service, interface, plugin and desktop identifiers together.
- [ ] Bump the context and launcher-guest protocol versions with Tettegouche.
- [ ] Document the migration.

**Done when:** one namespace across the suite and a clean upgrade from the old
identities.

## Block 11 — Host and public site

- [ ] HTTPS host, administration, startup, backups and uptime.
- [ ] The Shuffle desktop shell, guided entry and capability cards.
- [ ] Keyboard, touch, responsive, performance and accessibility testing.

**Done when:** the site shows the released product and a complete install path.

## Block 12 — Post-MVP debug

### 12a. Keyboard and the dock's room

- [x] Find whether KWin's keyboard adjustment can be declined for a managed
      window.
- [ ] Confirm no card, pane or window grows into the dock's room while the keys
      are up. (audit 14)

**Done when:** a keyboard episode changes no geometry but the Active card's
room for the keys.

### 12b. Edge gestures on any touchscreen

- [ ] Prove Spread and Active open from Plasma's own edges on a touchscreen
      that is not a Z13.
- [ ] Decide whether the bezel rule applies on every touchscreen, or Plasma's
      edges do.
- [ ] Make sure a user's own Plasma edge setting cannot fire alongside
      Kadunce's.

**Done when:** on a non-Z13 touchscreen, a bezel swipe opens Spread and Active,
and nothing else fires.

### 12c. Dock overflow

- [ ] Cap and condense the dock once working sets outgrow it.

### 12d. Give back to KDE

- [ ] J picks what to send from `UPSTREAM.md`.

### 12e. Monitor layout

Smarter layouts; the monitor's rule meets MVP.

- [ ] Let minimum sizes, not a count of eight, decide how many panes fit.
- [ ] A card carried onto a full monitor layout takes one slot, and that
      slot's window waits in the dock.
- [ ] When room frees up on the monitor, a window waiting in the dock comes
      back on its own.
- [ ] A low or high side snap makes room under a larger pane rather than
      taking a full-height third.
- [ ] Hand-test the monitor's layout rule.

### 12f. The keys on the first tap

- [ ] The keys come up for the first text field touched in a session. (audit 4)
- [ ] The keys come up for a text box tapped right after a card is chosen.
      (audit 5)

### 12g. After 1.0

- [ ] The calendar rising from the bottom edge as a card, with a timeline.

## Open decisions

1. **Plugin compatibility.** Approved: ship the KWin plugin as a package from a
   project pacman repository, built per KWin release, so a Plasma update that
   breaks the plugin is repaired by an ordinary system update rather than a
   rebuild on the user's machine. Scheduled in Block 10. See `DECISIONS.md`
   § A consumer gets a working plugin from the package manager.
2. **Shuffle Lock scope.** Decided 25 September. The lock shows the time, the
   credential field and the Keyboard's handle, and nothing from the session.
   See `../shuffle/docs/LOCK-CONTRACT.md`.
3. **Edge pairing grammar.** Decided 19 September. See `DECISIONS.md` § Spread
   order selects the partner; the gesture places it.
4. **What 1.0 promises about displays and desktops.** Decided 23 September.
   One touchscreen holds cards, wherever it is; every other display gets Bento,
   and a machine with no touchscreen gets Bento and Table only. 1.0 includes
   Table, so every desktop gets its own cards in Block 8, and cards stay on the
   desktop they started on until then. Audit 8 to 11.
5. **Table's release.** Decided 23 September: Table is in 1.0, replacing the
   earlier tentative 1.1. Audit 12.
6. **Which dock holds the monitor's windows.** Decided 24 September: one dock
   lists every display's windows. See `DECISIONS.md` § The platform's answer
   wins where it has one.

## Working model

Two session types share GitHub and have different reach.

| | Local session | Cloud session |
| --- | --- | --- |
| Full CTest, KWin-linked and D-Bus tests | yes | no |
| Package, control, install, physical review | yes | no |
| `tests/verify-headless.sh` domain suite | yes | yes |
| Reading, analysis, documentation | yes | yes |

Domain-layer work in `CardWorkspaceState`, `SpreadModel`, `SpreadLayout` and
`BentoLayout` is fully covered by the headless suite and can be prepared in
either session. Anything touching `Effect`, `WorkspaceInputRouter`, packaging or
the installed system needs a local session. A headless pass is a pre-check,
never promotion evidence. The product boundary, execution policy, planning controls and ordering
rationale are in `ROADMAP-CONTEXT.md`.

## Keeping this a checklist

When a task finishes, tick it here and update the current target. Anything worth
keeping about it --- measurements, a physical verdict, what was rejected and why
--- goes in `ROADMAP-CONTEXT.md` under the same block; durable decisions go in
`DECISIONS.md`. A line here stays one task. No session narration, test logs or
candidate hashes.
