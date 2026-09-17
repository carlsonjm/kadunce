# Shuffle launch roadmap

This is the sole canonical execution plan for Shuffle 1.0. Update status and order
when product scope, architecture, acceptance, or a blocking dependency changes. Git
records routine implementation steps and `docs/archive/` records dated evidence.

## Product boundary

- Good Input publishes Shuffle for Plasma.
- Kadunce, Tettegouche, and Temperance remain open-source component repositories.
- A private Shuffle repository will own consumer assembly, private assets, release
  packaging, product-facing naming, and support material.
- Table and Shuffle Keyboard are required 1.0 capabilities.
- KWin, Plasma, and the system input stack remain authoritative for windows,
  virtual desktops, and text delivery.

## Execution policy

- Keep one implementation owner per repository. Use a second worker only for
  read-only review or a disjoint file set.
- Identify and measure the property that controls a visual or geometry defect before
  changing constants.
- Batch two or three related physical checks into one candidate. Freeze passed items;
  later candidates touch only failures.
- Separate feasibility, minimal prototype, and product implementation for Table and
  Shuffle Keyboard.
- Keep `SWARM.md` empty unless another live worker must act.
- Update durable documentation only for accepted/rejected behavior, architecture or
  scope changes, sequencing blockers, installed provenance, and published freezes.
- Keep rejected candidates out of `main`; preserve useful evidence in the archive.
- Estimate remaining work only after the relevant feasibility proof.

## Current sequence

1. Finish Temperance notification action-pill spacing and validate or remove snooze.
2. Fix Kadunce Bento-derived Card Line live presentation geometry.
3. Finish Kadunce Card Line extraction, reordering, labels, and stack position.
4. Complete Tette responsive media/transfer and completion behavior.
5. Finish remaining component features and visual polish.
6. Audit maintainability and lifecycle boundaries.
7. Prove and implement Table.
8. Prove and implement Shuffle Keyboard.
9. Assemble, package, install, and release the private Shuffle product.
10. Prepare the website host and interactive release site.

Only run steps in parallel when they use different repositories, have disjoint
physical checks, and have explicit ownership.

## 1. Component correctness and polish

**Status:** In progress

### Kadunce

- [x] Cross-display Escape returns a carried window to its ownership origin.
- [x] Windows commit to Card Line ownership on entry without an Active detour.
- [x] Constrained windows route through Bento admission or prepared Active ownership.
- [x] Tablet Card Line retains Bento members as a stack and selects the large pane.
- [x] Output-local Bento ownership and lifecycle remain isolated.
- [ ] Make Bento-derived live Card Line content use the correct proportional
  presentation geometry without retained snapshots or native Bento resizing.
- [ ] Make stack extraction reliable.
- [ ] Give reorder a useful intent zone without accidental paging.
- [ ] Center card labels and place stack position on the same row at the right edge.

### Tettegouche

- [ ] Remove media metadata in priority order: artist, song, then previous/next;
  keep play/pause until last.
- [ ] Recompute media width when transfer activity appears so content does not run
  under the application dock.
- [ ] Keep pause and authoritative completion feedback visible long enough to use.
- [ ] Complete drawer bounds, sorting, hover/active states, Files visuals, and shared
  control spacing.

### Temperance

- [x] Apply the accepted notification hierarchy, lowercase labels, compact power
  glyphs, hairline/hover-fill actions, and popup clearance.
- [ ] Refine action-pill typography or horizontal padding.
- [ ] Verify that snooze reaches a real notification action and preserves state;
  remove it if the route is not functional.
- [ ] Keep recent notifications distinct and fit popup geometry beside adjacent
  cards.
- [ ] Align Control Center and System Tray with the shared spacing hierarchy.

Tette owns brief authoritative activity completion. Temperance may show the completed
transition only when it remains useful or actionable; both sides must not retain the
same completion indefinitely.

**Exit gate:** No missing windows, stuck input, broken restoration, cross-output
ownership leak, or failed Kadunce disable control. Physical review passes the exact
candidate on supported hardware.

## 2. Remaining component features

**Status:** Queued

### Files discovery and properties

- [ ] File and folder Properties.
- [ ] Thumbnails and previews.
- [ ] Recursive search and recent files.
- [ ] Loading, empty, and failure states.

External drag-and-drop and Open With are outside the 1.0 plan.

### Storage lifecycle

- [ ] Present removable-device arrival and removal.
- [ ] Provide safe mount/unmount behavior.
- [ ] Add a bounded truthful subset of KIO/network locations if feasible.
- [ ] Handle disappearance, reconnect, and failure without stale state.

### Temperance event sources

- [ ] Completed transfer.
- [ ] Device and network transition.
- [ ] Relevant battery or system-state threshold.
- [ ] Ephemeral event treatment distinct from Tette live activities.

### Steam and external libraries

- [ ] Discover installed Steam libraries if this slice remains in 1.0.
- [ ] Delegate launch and Proton selection to Steam.
- [ ] Handle unavailable drives safely.
- [ ] Avoid custom compatibility or boot-mount management.

This is the first slice to defer if release pressure requires scope reduction.

**Exit gate:** Each provider or lifecycle slice has focused automated checks and an
installed acceptance pass. One provider failure does not block unrelated slices.

## 3. Maintainability and lifecycle audit

**Status:** Queued

- [ ] Remove accidental duplication that creates visible reliability or maintenance
  risk.
- [ ] Verify Tette/Temperance activity ownership and cleanup boundaries.
- [ ] Audit timers, model lifetime, responsive calculations, and resource paths.
- [ ] Confirm protected custom icons remain unchanged.
- [ ] Make Kadunce custom motion respect platform animation scaling and reduced
  motion.
- [ ] Keep the audit bounded; do not reopen stable architecture for style alone.

**Exit gate:** The three components are lifecycle-safe, maintainable, and ready for
private product consumption without duplicated state ownership.

## 4. Table

**Status:** Product contract approved; engineering feasibility queued

Table provides the level above Card Line/Bento:

`Active = this window → Card Line/Bento = these windows → Table = these workspaces`

- [ ] Audit KWin/Plasma virtual-desktop APIs, gesture ownership, and lifecycle.
- [ ] Define touch entry/exit, workspace presentation, and direct manipulation.
- [ ] Prove multi-display behavior without changing output-local Bento semantics.
- [ ] Preserve KWin as the virtual-desktop authority.
- [ ] Build the smallest complete prototype and test it physically.
- [ ] Add restoration, interruption, safety, and rollback behavior.
- [ ] Implement the accepted interaction only after feasibility passes.

Reference: `KADUNCE-TABLE-1.1-CONCEPT.md`.

**Exit gate:** A user can enter Table, move a real managed window between existing
KDE virtual desktops, enter the destination Card Line, and observe correct underlying
desktop membership without regressing normal KDE switching or display ownership.

## 5. Shuffle Keyboard

**Status:** Product contract approved; technical evaluation queued

- [ ] Audit Plasma Keyboard, Qt Virtual Keyboard, KWin input-method plumbing, and
  Fcitx5 OSK.
- [ ] Select a system-backed implementation base or document why none is viable.
- [ ] Prove the four-row layout, cascading Backspace/Enter/Shuffle controls, and
  directly adjustable height.
- [ ] Prove the full-footprint keyboard/precision-surface transition.
- [ ] Validate edit gestures separately from pointer behavior.
- [ ] Verify locale/keymap correctness, focus, latency, and loss-free input across
  Qt/KDE, GTK, browsers, Electron, and terminals.
- [ ] Reserve usable workspace correctly as height changes.
- [ ] Keep autocorrect, prediction, swipe typing, dictation, and custom IME work out
  of 1.0 unless mature system infrastructure supplies them.

Reference: `SHUFFLE-KEYBOARD-1.0-CONCEPT.md`.

**Exit gate:** A supported 10–13 inch touch device can perform reliable text and
precision desktop input without physical peripherals. Dropped characters, meaningful
latency, wrong keymaps, focus loss, or unreliable show/hide behavior block release.

## 6. Private product assembly and installation

**Status:** Queued after Table and Keyboard feasibility

- [ ] Establish the Good Input organization and private Shuffle repository.
- [ ] Document the public component/private product boundary.
- [ ] Consume pinned component versions with provenance.
- [ ] Apply Shuffle product naming while preserving component compatibility.
- [ ] Build from clean component and integration checkouts.
- [ ] Provide one Fish-safe installer with dependency detection.
- [ ] Verify versioning, upgrade, rollback, uninstall, and Kadunce safety control.
- [ ] Complete a fresh-machine installation test on the supported environment.

**Exit gate:** A fresh supported machine can install, update, roll back, and uninstall
Shuffle without repository knowledge, while the Kadunce disable control remains
functional throughout.

## 7. Website host

**Status:** Queued

- [ ] Configure the Mac mini as a standard HTTPS website host.
- [ ] Establish secure standard tablet administration for deploy, status, logs,
  restart, and rollback.
- [ ] Configure startup, backups, and basic uptime/disk visibility.
- [ ] Document DNS, router, and recovery dependencies.

The host is not a custom orchestration product, build farm, or artifact service.

## 8. Interactive website and release

**Status:** Queued

- [ ] Build the full-screen Shuffle desktop shell and guided entry.
- [ ] Present active Kadunce, Tettegouche, and Temperance capability cards.
- [ ] Add representative interactions that explain the product without simulating
  unsupported behavior.
- [ ] Provide installation, requirements, source, privacy, support, and release
  information.
- [ ] Test keyboard, touch, responsive layout, performance, and accessibility.

**Exit gate:** The public site accurately demonstrates the released product and
provides a complete supported installation path.

## Progress update rule

For a meaningful accepted block, update only:

- its checkbox or status;
- any changed invariant or scope decision;
- the next blocked dependency;
- the estimate when new feasibility evidence makes it defensible.

Do not append sprint narration, worker summaries, candidate hashes, or repeated test
logs to this file.
