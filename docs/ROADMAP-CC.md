# Shuffle execution plan

The checklist for the whole suite, in the order the work is done. One line per
task: `[x]` done, `[ ]` open. Why a task exists, what was measured and what it
needs to know live in `ROADMAP-CONTEXT.md` under the same block heading.

## Next

1. Make the monitor organize everything it shows (Block 14).
2. Rebuild the Keyboard against the 22 September direction (Block 9).
3. Make the keys come up for the first text field of a session (Block 13).
4. Show an app whose dialog is waiting in Ambient (Block 6).

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
| 13 | What a person meets first | Next |
| 14 | Ownership scope | Next: build the monitor's layout rule |
| 9 | Shuffle Keyboard | In progress |
| 5 | Bottom Surface | In progress; the Keyboard's arrival waits on Block 9's rebuild |
| 15 | Consumer fundamentals | Ready: options for J |
| 4 | Kadunce manipulation | In progress |
| 16 | Motion | Ready |
| 6 | Tettegouche completion | Width work waits on Block 5; the rest is ready |
| 7 | Temperance event boundary | Ready |
| 6b | Maintainability and lifecycle audit | Ready |
| 7b | Spread deck motion | Waits on Blocks 5, 6 and 7 |
| 9b | Shuffle Lock | Waits on Block 9; ships in the consumer bundle |
| 10 | Downstream assembly and installation | Waits on Block 9 |
| 10b | Package and interface identity | Ready; coordinated across all three repositories |
| 11 | Host and public site | Waits on Block 10 |
| 8 | Table | In 1.0; keeps its place in the order |
| 12 | Post-MVP debug | Deferred past MVP |

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
- [ ] The keys come up for the first text field touched in a session. (audit 4)
- [ ] The keys come up for a text box tapped right after a card is chosen.
      (audit 5)
- [x] The first drag of a window after Kadunce is switched on adopts it, as the
      second does. (audit 6)

**Done when:** on the tablet, a person searches, saves a file, and types into a
freshly chosen card, and nothing goes wrong.

## Block 14 — Ownership scope

- [x] Measure what a virtual-desktop switch does while cards are owned.
      (audit 9)
- [x] Keep cards and layouts on the desktop they started on, and leave every
      other desktop plain Plasma, until Table. (audit 9)
- [x] J rules the desktop half: 1.0 includes Table, so every desktop gets its
      own cards. (audit 11)
- [x] Measure what a touch monitor gets, and a laptop whose built-in screen is
      not touch. (audit 8)
- [x] J rules the display half: one touchscreen holds cards, and a machine
      with none gets Bento and Table only. (audit 11)
- [x] Put cards on the screen the touchscreen drives, not the one named like a
      laptop panel. (audit 8)
- [x] Keep the tablet's cards whole and its bottom swipe working when a
      monitor is plugged in or out.
- [x] J rules the monitor: it organizes everything it shows, and a window
      without room goes to the dock. (audit 10)
- [x] Make the card and product contracts say what is ruled. (audit 10)
- [ ] **Next:** send a window the monitor's layout has no room for to the dock,
      never to the tablet, and never leave one loose beside a layout.
- [ ] Let minimum sizes, not a count of eight, decide how many panes fit.
- [ ] One window on the monitor: a side snap takes half, a top snap takes the
      Active card's size.
- [ ] Hand-test the monitor's layout rule.

**Done when:** the contracts, the current state and the build agree for every
display and desktop a person can reach.

## Block 9 — Shuffle Keyboard

- [x] Audit Plasma Keyboard, Qt Virtual Keyboard, KWin input methods and Fcitx5.
- [ ] **Next:** rebuild against the 22 September direction and put it back in
      daily use, with an even gap at the sides and the old top grab retired.
      (audit 13)
- [ ] Bring J how the keys are put away once the top grab retires.
- [ ] Bring the concept's height and reservation lines in line with keys that
      cover rather than shrink. (audit 14)
- [ ] Prove the four-row layout, controls, height, and the switch to the
      precision surface.
- [ ] Verify locales, keymaps, focus, latency and loss-free input across Qt/KDE,
      GTK, browsers, Electron and terminals. (audit 24)
- [ ] Bring J options for copy and paste by touch. (audit 29)
- [x] Lay the Keyboard over cards: a covered line pans inside the Active card,
      and a pull never takes the focus.
- [ ] Keep autocorrect, prediction, swipe typing and dictation out of 1.0.

**Done when:** a 10 to 13 inch touch device types and points reliably with no
physical peripherals.

## Block 5 — Bottom Surface

- [x] Integration repository exists (Block 10a).
- [x] Author the Bottom Surface contract.
- [x] Settle what hosts Temperance once no Plasma panel exists.
- [x] Shuffle Dock: centred, pin, reorder, the standard right-click menu.
- [x] Keyboard boundary: the dock steps aside and the Keyboard comes all the way
      down.
- [x] Keyboard handle: six pixels on the dock, works on the first pull after
      signing in, and a pull from anywhere on the dock raises the Keyboard.
- [x] Spread opens only from the bezel.
- [x] A tap on a dock app opens it; the menu is a long press.
- [x] An app with several windows lists them in its dock menu, and a tap
      brings back the one used last.
- [x] The gradient arrives on the tablet panel with no visible banding.
- [ ] Give the Keyboard its arrival motion, so it and the dock move as one
      action, once Block 9's rebuild has set the Keyboard's shape.
      (audit 13, 36)
- [ ] Retest the asymmetric Ambient and ticker width.
- [ ] Once the flanks hold, retire the width fixes they made redundant.
      (audit 20)
- [ ] Give Temperance clock and calendar presentation.
- [ ] Keep the ticker legible on a bright wallpaper.
- [ ] Replace the gradient with a solid fill under reduced transparency.
      (audit 35)

**Done when:** the dock is centred and grows evenly at tablet and monitor widths,
and the Status Bar and Ambient each fill the space the dock leaves them.

## Block 15 — Consumer fundamentals

Each line brings J options first. An accepted answer becomes a task in the block
that owns it.

- [ ] A gesture shows its result before release, and a refused one says so.
      (audit 22)
- [ ] Come back the way it was left after sign-out or restart. (audit 23)
- [ ] Close an app by touch from Spread. (audit 25)
- [ ] Type into text the keys cover: Bento panes, terminals, and boxes at the
      bottom of a window. (audit 19)
- [ ] What a pull down from the top edge brings. (audit 26)
- [ ] Whether attaching the keyboard changes the mode. (audit 27)
- [ ] Right-click and pointing without raising the keys. (audit 28)
- [ ] One place to find everything. (audit 31)
- [ ] Check portrait, and sleep and wake, on the tablet. (audit 30)

**Done when:** every line has J's ruling, and every accepted answer is a task in
its block.

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

## Block 16 — Motion

- [ ] Complete arrival, displacement, cancellation and neighbor motion.
      (audit 21)
- [ ] Panned contents slide rather than jump. (audit 37)
- [ ] Make compositor motion follow animation scaling and reduced motion.
      (audit 35)
- [ ] Review the Keyboard's arrival, the dock stepping aside and card motion
      together. (audit 21)

**Done when:** every custom motion in the suite uses the shared timings and
honours reduced motion.

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

## Block 9b — Shuffle Lock

- [ ] Write the product contract (see Open decisions).
- [ ] Keep KDE's screen locker as the authentication authority.
- [ ] Prove the Keyboard works on the lock screen.
- [ ] Define failure, interruption, timeout and multi-display behavior.

**Done when:** the lock conceals the workspace, authenticates through the system
locker, and a Shuffle failure never leaves a session unlocked.

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

## Block 8 — Table

- [ ] Audit KWin and Plasma virtual-desktop APIs, gestures and lifecycle.
- [ ] Give every virtual desktop its own cards and layouts, replacing the
      one-desktop rule. (audit 9)
- [ ] Prove multi-display behavior without changing Bento.
- [ ] Build the smallest complete prototype and test it physically.
- [ ] Implement the accepted interaction.
- [ ] Bring J the option of a monitor layout's overflow going to another desktop
      through Table instead of the dock.

**Done when:** a window moves between real KDE desktops through Table without
breaking normal desktop switching.

## Block 12 — Post-MVP debug

### 12a. Keyboard and the dock's room

- [x] Find whether KWin's keyboard adjustment can be declined for a managed
      window.
- [ ] Confirm no card, pane or window grows into the dock's room while the keys
      are up. (audit 14)
- [ ] A tap just above a panned card reaches nothing the pan hid. (audit 37)

**Done when:** a keyboard episode changes no card or pane geometry, and nothing
hidden takes a tap.

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

## Open decisions

1. **Plugin compatibility.** Approved: ship the KWin plugin as a package from a
   project pacman repository, built per KWin release, so a Plasma update that
   breaks the plugin is repaired by an ordinary system update rather than a
   rebuild on the user's machine. Scheduled in Block 10. See `DECISIONS.md`
   § A consumer gets a working plugin from the package manager.
2. **Shuffle Lock scope.** Open. What the lock screen hides, and what stays
   usable before signing in. The lock ships in the consumer bundle whatever else
   1.0 holds.
3. **Edge pairing grammar.** Decided 19 September. See `DECISIONS.md` § Spread
   order selects the partner; the gesture places it.
4. **What 1.0 promises about displays and desktops.** Decided 23 September.
   One touchscreen holds cards, wherever it is; every other display gets Bento,
   and a machine with no touchscreen gets Bento and Table only. 1.0 includes
   Table, so every desktop gets its own cards in Block 8, and cards stay on the
   desktop they started on until then. Audit 8 to 11.
5. **Table's release.** Decided 23 September: Table is in 1.0, replacing the
   earlier tentative 1.1. It keeps its place in the order. Audit 12.

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

When a task finishes, tick it here and move the Next list on. Anything worth
keeping about it --- measurements, a physical verdict, what was rejected and why
--- goes in `ROADMAP-CONTEXT.md` under the same block; durable decisions go in
`DECISIONS.md`. A line here stays one task. No session narration, test logs or
candidate hashes.
