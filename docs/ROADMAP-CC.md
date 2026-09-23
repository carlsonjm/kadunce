# Shuffle execution plan

The checklist for the whole suite, in the order the work is done. One line per
task: `[x]` done, `[ ]` open. Why a task exists, what was measured and what it
needs to know live in `ROADMAP-CONTEXT.md` under the same block heading.

## Next

1. Give the Keyboard's arrival motion, so the Keyboard and the dock move as one
   action (Block 5).
2. Retest the asymmetric Ambient and ticker width (Block 5).
3. Give Temperance clock and calendar presentation (Block 5).
4. Answer the Bottom Surface's verification owed (Block 5).
5. Rebuild the Keyboard against the 22 September direction (Block 9).

## Blocks

| Block | | Status |
| --- | --- | --- |
| 1 | Refactor enablement | Done |
| 2 | Ownership foundation | Done |
| 3 | Ownership behavior | Done |
| 3b | Bento layout grammar | Done |
| 4 | Kadunce manipulation | In progress |
| 5 | Bottom Surface | In progress |
| 6 | Tettegouche completion | Width work waits on Block 5; the rest is ready |
| 6b | Maintainability and lifecycle audit | Ready |
| 7 | Temperance event boundary | Ready |
| 7b | Spread deck motion | Waits on Blocks 5, 6 and 7 |
| 8 | Table | Waits on Blocks 3 and 4 |
| 9 | Shuffle Keyboard | In progress |
| 9b | Shuffle Lock | Waits on Block 9 |
| 10a | Integration repository | Done |
| 10 | Downstream assembly and installation | Waits on Blocks 9 and 10a |
| 10b | Package and interface identity | Waits on Block 10a |
| 11 | Host and public site | Waits on Block 10 |
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

## Block 4 — Kadunce manipulation

- [x] A resumed layout survives a pane that will not take its rect back.
- [x] Make stack extraction reliable.
- [x] Normalize live terminology to Spread.
- [ ] Give reordering a usable intent zone without accidental paging.
- [ ] Let a Spread drop onto the Bento group name the pane it replaces.
- [ ] Displace by side.
- [ ] First Card or Bento entry adopts every eligible window on that display and
      desktop at once.
- [ ] Complete arrival, displacement, cancellation and neighbor motion.
- [ ] Make compositor motion follow animation scaling and reduced motion.

**Done when:** manipulation passes physical review on supported hardware.

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
- [ ] **Next:** give the Keyboard's arrival motion, so it and the dock move as
      one action.
- [ ] Retest the asymmetric Ambient and ticker width.
- [ ] Give Temperance clock and calendar presentation.
- [ ] Answer the verification owed: gradient banding, blur smear line, ticker
      legibility on a bright wallpaper, and a solid fill for reduced
      transparency.

**Done when:** the dock is centred and grows evenly at tablet and monitor widths,
and the Status Bar and Ambient each fill the space the dock leaves them.

## Block 6 — Tettegouche completion

- [ ] Answer a virtual keyboard: the layout has to work with the Keyboard up.
- [ ] Ambient release validation against `AMBIENT-CONTRACT.md`.
- [ ] Media metadata priority and dock-aware width.
- [ ] Transfers: pause, completion, and conflict choices that never overwrite
      silently.
- [ ] Files: properties, previews, recursive search, Recent, error states.
- [ ] Storage lifecycle without weakening KIO ownership.

**Done when:** each slice has automated checks and an installed acceptance pass.

## Block 6b — Maintainability and lifecycle audit

- [ ] Remove duplication that creates reliability or maintenance risk.
- [ ] Verify Tettegouche and Temperance activity ownership and cleanup.
- [ ] Audit timers, model lifetime, responsive calculations and resource paths.
- [ ] Confirm the protected custom icons are unchanged.

**Done when:** the three components are lifecycle-safe with no duplicated state.

## Block 7 — Temperance event boundary

- [ ] Add only authoritative, useful system events, deduplicated and dismissable.
- [ ] Keep live progress and actions with Tettegouche.
- [ ] Revisit presenter switching once its lifecycle is proven.
- [ ] Refine action-pill typography and padding.

**Done when:** events are authoritative and never duplicated.

## Block 7b — Spread deck motion

- [ ] The row follows the finger, coasts and snaps to the nearest card.
- [ ] Rebuild browsing on that motion.
- [ ] Decide the row's shape.
- [ ] Retire edge-dwell paging once a throw reaches a distant card.
- [ ] Reorder with the deck rather than a third distance.
- [ ] Draw a held card from under the finger.
- [ ] Give the row something that shows its order.

**Done when:** dragging and throwing the deck settles where the hand expects, and
reordering and paging are never confused.

## Block 8 — Table

- [ ] Audit KWin and Plasma virtual-desktop APIs, gestures and lifecycle.
- [ ] Prove multi-display behavior without changing Bento.
- [ ] Build the smallest complete prototype and test it physically.
- [ ] Implement the accepted interaction.

**Done when:** a window moves between real KDE desktops through Table without
breaking normal desktop switching.

## Block 9 — Shuffle Keyboard

- [x] Audit Plasma Keyboard, Qt Virtual Keyboard, KWin input methods and Fcitx5.
- [ ] Rebuild against the 22 September direction and put it back in daily use.
      J's open asks: an even gap at the Keyboard's sides, a gap above the keys,
      and retire the old top grab handle.
- [ ] Prove the four-row layout, controls, height, and the switch to the
      precision surface.
- [ ] Verify locales, keymaps, focus, latency and loss-free input across Qt/KDE,
      GTK, browsers, Electron and terminals.
- [ ] Reserve workspace correctly as the height changes.
- [ ] Keep autocorrect, prediction, swipe typing and dictation out of 1.0.

**Done when:** a 10 to 13 inch touch device types and points reliably with no
physical peripherals.

## Block 9b — Shuffle Lock

- [ ] Write the product contract (see Open decisions).
- [ ] Keep KDE's screen locker as the authentication authority.
- [ ] Prove the Keyboard works on the lock screen.
- [ ] Define failure, interruption, timeout and multi-display behavior.

**Done when:** the lock conceals the workspace, authenticates through the system
locker, and a Shuffle failure never leaves a session unlocked.

## Block 10a — Integration repository

Done, 21 September.

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

### 12a. Keyboard geometry and the stage gutter

- [ ] Log what placement is asked for and what survives, on every keyboard
      change.
- [ ] Find whether KWin's keyboard adjustment can be declined for a managed
      window.
- [ ] Hold the dock's space through a keyboard raise.
- [ ] Give the Active card the motion the panes already have.

**Done when:** a keyboard raised over a layout or a card keeps the gutter and
moves once, smoothly.

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

1. **Plugin compatibility.** Approved: ship the KWin plugin as a package, built
   per KWin release. Scheduled in Block 10.
2. **Shuffle Lock scope.** Open. What the lock screen hides, and what stays
   usable before signing in.
3. **Edge pairing grammar.** Decided 19 September. See `ROADMAP-CONTEXT.md`.

## Working model

Two session types share GitHub and have different reach.

| | Local session | Cloud session |
| --- | --- | --- |
| Full CTest, KWin-linked and D-Bus tests | yes | no |
| Package, control, install, physical review | yes | no |
| `tests/verify-headless.sh` domain suite | yes | yes |
| Reading, analysis, documentation | yes | yes |

Anything touching `Effect`, `WorkspaceInputRouter`, packaging or the installed
system needs a local session. A headless pass is a pre-check, never promotion
evidence. The product boundary, execution policy, planning controls and ordering
rationale are in `ROADMAP-CONTEXT.md`.

## Keeping this a checklist

When a task finishes, tick it here and move the Next list on. Anything worth
keeping about it --- measurements, a physical verdict, what was rejected and why
--- goes in `ROADMAP-CONTEXT.md` under the same block; durable decisions go in
`DECISIONS.md`. A line here stays one task. No session narration, test logs or
candidate hashes.
