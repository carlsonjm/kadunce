# Shuffle launch roadmap — rolling execution plan

Updated September 16, 2026. This is the active roadmap and status format for the
remainder of Shuffle 1.0. Update each section as work is accepted. After every
meaningful block, record actual elapsed time and revise the remaining planning
number from observed delivery speed.

J owns product priority and physical acceptance. B-Team/Sol is the default bounded
implementation owner. A-Team/Astra owns cross-suite architecture, Kadunce, risky
lifecycle work and ownership gates. Repo source remains authoritative.

## Planning snapshot

- **Current state:** paused for working-budget replenishment
- **Previous implementation forecast:** 5.5 focused days for the former Itasca
  component/installer/website scope; retired because it excluded Table, Keyboard,
  private-product assembly and the Good Input / Shuffle migration
- **Next estimate gate:** after bounded Table and Keyboard feasibility plus the
  private consumer-repository plan
- **Release priority:** get the first consumer release right; no date-driven cut
  is allowed to remove an essential interaction merely to launch sooner

The September 15 baseline was faster than the original estimate: the major
Ambient architecture, real providers, design system, physical corrections and
suite freeze landed in roughly six hours at about 20% weekly usage. The planning
number is therefore historical evidence, not the current launch forecast.

## Product and repository boundary

- **Good Input** is the company and publisher.
- **Shuffle for Plasma** is the consumer product; **Shuffle** is the conversational
  name.
- **Kadunce, Tettegouche and Temperance** remain the internal component names and
  the free/open-source repositories.
- A Good Input organization and private Shuffle consumer repository will own the
  integrated commercial product layer, release assembly, consumer assets and
  product-facing packaging. Preserve component Git history and compatibility.
- The current checkout may remain physically named `Itasca` until a controlled path
  migration is scheduled; product-facing work uses Shuffle immediately.

## Lean execution policy

- Keep one implementation owner per repository. A second agent may perform a
  read-only review or a separately scoped task on disjoint files.
- Route bounded implementation, visual correction, tests and packaging to Sol.
  Route compositor ownership, lifecycle, cross-suite contracts and failed-candidate
  diagnosis to Astra. PM owns sequencing, scope, documentation and publication.
- Before changing visual or geometry behavior, identify and measure the property
  that controls the rendered result. A plausible constant is not evidence.
- Package two or three closely related physical checks into one candidate. Freeze
  passed items and let the next candidate touch only failed items.
- Separate feasibility, minimal prototype and product implementation for Table and
  Keyboard. Research must not silently become an implementation branch.
- Keep `SWARM.md` empty unless another live agent must act. Backlog, rejected
  candidates and pending physical review belong in canonical state documents.
- Update durable documentation only for acceptance/rejection, scope or architecture
  changes, sequencing blockers and published freezes. Git records routine steps.
- Assignment prompts should name the roadmap item, preserved boundaries, stop
  condition, validation and physical checklist; agents obtain context from the repo.
- Keep rejected candidates out of `main`. Preserve only useful evidence and resume
  them through a new bounded assignment.
- Re-estimate from completed evidence and feasibility results, never from an old
  launch target.

## Resume sequence after budget replenishment

1. Close the remaining Temperance notification action-pill typography/padding and
   verify or remove the snooze action path.
2. Fix Kadunce Bento-derived Card Line live presentation geometry. This remains the
   gate before further Tette feature work.
3. Complete Card Line extraction, reordering, labels and stack-position behavior.
4. Resume Tette responsive media/transfer and completion behavior.
5. Finish the remaining component polish and feature sections in roadmap order.

Dispatch one bounded packet at a time unless tasks use different repositories,
disjoint physical checks and an explicit token budget supports parallel work.

## 1. Debug and polish

**Status:** In progress
**Estimate:** 1–1.5 days
**Authoritative checklist:** `POST-FREEZE-TEST-20260915.md`

### A-Team — behavioral regressions

- [x] Cross-display Escape release returns the window to the tablet ownership origin.
- [x] Windows commit to Card Line ownership on entry without first becoming Active.
- [x] Temperance ticker is no longer confined to the arrow-control view box.
- [x] New constrained windows route through the Bento solver or prepared Active
  ownership instead of floating unmanaged above Bento.
- [x] Touch Card Line preserves Bento-owned cards as a stack with the large-pane
  card on top, without changing monitor Bento behavior.
- [ ] Bento-derived stack cards use correct live proportional presentation
  geometry in Card Line without retained snapshots or native Bento resizing.
- [ ] Pulling a card from a stack back into Card Line is reliable.
- [ ] Reordering has a useful intent zone without accidental paging.
- [ ] Cards have centered labels; stack position shares that row at the right edge.

### B-Team — Ambient, Temperance and visual polish

- [ ] Ambient sheds artist, song, then previous/next; play/pause survives last.
- [ ] A transfer recomputes available media width instead of pushing content under
  the application dock.
- [ ] Pause and authoritative completion feedback remain visible long enough to use.
- [ ] Recent notifications remain distinct and popup geometry fits neighboring cards.
- [x] Notification cards follow the design-kit hierarchy: source and title share one
  header row, body copy sits directly below, action pills use tighter padding, and
  card spacing is recalculated around the simplified layout.
- [x] `do not disturb` is lowercase.
- [x] Power glyphs are slightly smaller.
- [x] Popup-to-dock clearance matches the accepted external spacing.
- [ ] Refine notification action-pill typography or side padding; J accepted the
  current hairline/hover-fill treatment as the paused baseline, but the label needs
  either smaller text or more breathing room at the sides.
- [ ] Verify that the newly exposed snooze action reaches a real notification path
  and preserves state correctly; keep it only if the action is functional.
- [ ] Control Center and System Tray follow the design-kit spacing hierarchy.
- [ ] Drawer reveal, dropdown bounds, sorting, hover/active pills and Files visuals
  align with the accepted kit.

### Completion handoff

Tette owns the brief authoritative completion state while an activity finishes.
Temperance may then surface the completed transition when it remains useful or
actionable. Do not leave duplicate persistent completion on both sides.

### Exit gate

A1, A2 ownership and T1 ticker now passed J's installed physical review.
The September 16 checkpoint closes constrained launch ownership, stack retention /
large-pane selection, monitor isolation/lifecycle, and the ticker regression with
expanding spacers. Zen may use the small pane when its minimum fits. The remaining
first-section gates are Bento-derived Card Line presentation, Tette center/transfer
behavior, card manipulation and notification/visual polish. Any missing window,
stuck input, or broken Kadunce disable control blocks release immediately.

See `CHECKPOINT-20260916.md`. Its 5.5-day number records the former scope and must
not be used as a Shuffle launch estimate now that Table, Keyboard and private-product
assembly are essential. Exact September 16 engineering/testing hours and weekly
usage were not measured; no invented actuals are recorded.

## 2. Missing features

**Status:** Queued
**Estimate:** 1.25–2.5 days

### Files B — properties and discovery

**Estimate:** 0.5–0.75 day

- [ ] File and folder Properties
- [ ] Thumbnails and previews
- [ ] Recursive search
- [ ] Recent files
- [ ] Loading, empty and failure states

The former Files A desktop-integration slice is deleted. External drag-and-drop
and Open With are not launch-roadmap items. Properties now belongs to Files B.

### Files C — storage lifecycle

**Estimate:** 0.5–1 day

- [ ] Removable-device arrival and removal
- [ ] Safe mount/unmount presentation
- [ ] Optional KIO/network locations
- [ ] Source disappearance, reconnect and truthful failure behavior

Network support should ship as a truthful bounded subset. Provider-specific
authentication must not turn this into a compatibility campaign.

### Temperance event sources

**Estimate:** 0.5–0.75 day

- [ ] Completed-transfer event
- [ ] Device and network transitions
- [ ] Battery or relevant system-state thresholds
- [ ] Ephemeral event presentation distinct from Tette live activities

### Steam and external libraries

**Estimate:** 0.5–1 day if retained for 1.0

- [ ] Discover installed Steam libraries
- [ ] Defer launch and Proton resolution to Steam
- [ ] Handle unavailable drives safely
- [ ] Avoid custom compatibility or boot-mount management

This remains the cleanest feature to move behind launch if schedule pressure grows.

### Exit gate

Each slice receives its own focused tests and installed acceptance. One provider
or storage failure must not hold unrelated slices open.

## 3. Refactor audit

**Status:** Queued
**Estimate:** 0.5–0.75 day

### Refactor audit

- [ ] Remove accidental duplication introduced during the sprint.
- [ ] Verify Tette/Temperance activity ownership and cleanup boundaries.
- [ ] Audit timers, model lifetime, responsive calculations and resource paths.
- [ ] Confirm protected custom icons remain intact.
- [ ] Review Kadunce animation scaling/reduced-motion behavior.
- [ ] Restrict fixes to visible reliability, maintenance or release risk.

This is a surgical audit, not a rewrite.

### Exit gate

The three open-source components are maintainable, lifecycle-safe and ready to be
consumed by the private Shuffle product without accidental duplicated ownership.

## 4. Table — essential consumer spatial model

**Status:** Concept locked; engineering feasibility queued
**Estimate:** Recalculate after feasibility

Table is part of the final consumer product, not a post-launch idea:

`Active = this window → Card Line/Bento = these windows → Table = these workspaces`

- [ ] Audit KWin/Plasma virtual-desktop APIs, gesture ownership and lifecycle.
- [ ] Define touch entry/exit, workspace presentation and direct manipulation.
- [ ] Prove multi-display behavior without changing monitor-only Bento semantics.
- [ ] Preserve KWin/Plasma as authority for virtual desktops.
- [ ] Prototype the smallest complete Table interaction and physically test it.
- [ ] Add safety, restore and rollback behavior before product integration.

See `KADUNCE-TABLE-1.1-CONCEPT.md`. The former post-1.0 classification is retired.

### Exit gate

J can manage workspaces through Table as the natural level above Card Line/Bento,
with correct touch, keyboard, lifecycle and multi-display behavior.

## 5. Shuffle Keyboard — essential consumer input surface

**Status:** Product concept approved; technical evaluation queued
**Estimate:** Recalculate after feasibility

Shuffle Keyboard completes the premium touch experience through reliable text
input and precision desktop control without requiring physical peripherals.

The authoritative product contract is `SHUFFLE-KEYBOARD-1.0-CONCEPT.md`. This
roadmap summarizes sequencing and does not replace that brief.

- [ ] Audit Plasma Keyboard, Qt Virtual Keyboard, KWin input-method plumbing and
  Fcitx5 OSK before selecting an implementation base.
- [ ] Use the four-row layout, cascading Backspace/Enter/Shuffle controls and
  directly adjustable height defined in the approved product brief.
- [ ] Prove the full-footprint Keyboard ↔ Shuffle precision-surface transition.
- [ ] Validate edit gestures separately from pointer/trackpad behavior.
- [ ] Verify locale/keymap correctness, focus stability, latency and dropped-input
  behavior across Qt/KDE, GTK, browsers, Electron and terminals.
- [ ] Reserve usable workspace correctly as keyboard height changes.
- [ ] Keep autocorrect, prediction, swipe typing, dictation, custom IMEs and similar
  language-engine work outside Shuffle 1.0 unless mature system infrastructure
  supplies it safely.

### Exit gate

A supported 10–13 inch touch device can perform reliable text and precision desktop
input without a physical keyboard or mouse. Dropped characters, meaningful latency,
wrong keymaps, focus loss or unreliable show/hide behavior block release.

## 6. Private Shuffle consumer product and installation

**Status:** Queued
**Estimate:** Recalculate after Table/Keyboard feasibility

- [ ] Establish the Good Input organization and private Shuffle repository.
- [ ] Record the boundary between open-source components and private product code,
  assets, integration, release assembly and support material.
- [ ] Consume pinned Kadunce, Tettegouche and Temperance versions with provenance.
- [ ] Apply Good Input / Shuffle naming to product-facing metadata and UI while
  preserving internal component names and upgrade compatibility.
- [ ] Build cleanly from all component repositories and the private integration repo.
- [ ] Provide one Fish-safe installation path with dependency detection.
- [ ] Include Plasma restart/session instructions in the completed flow.
- [ ] Verify versioning, upgrades, rollback, uninstall and Kadunce safety control.
- [ ] Complete a fresh-user installation test on the supported Plasma environment.

### Exit gate

A fresh supported machine can install, update, roll back and uninstall Shuffle
without repository knowledge. The public component boundary remains truthful and
Kadunce's live disable control remains functional throughout.

## 7. Mac mini website host and tablet control

**Status:** Queued
**Estimate:** 0.25–0.5 day, excluding router/DNS surprises

The Mac mini only hosts the Shuffle website and its repository. J's personal site
may move there later, but that migration does not block Shuffle launch.

- [ ] Create the website repository checkout and production build directory.
- [ ] Configure a standard web server and HTTPS deployment path.
- [ ] Establish secure standard remote control from the tablet.
- [ ] Support deploy, restart, logs and rollback from the tablet.
- [ ] Configure startup, backups and basic uptime/disk visibility.
- [ ] Document DNS/router dependencies and recovery steps.

No custom server dashboard, orchestration product, build farm or artifact service.
Use established remote administration and deployment tools.

### Exit gate

From the tablet, J can securely deploy or roll back the site, inspect status/logs,
and restart the standard website service.

## 8. Interactive website and release

**Status:** Queued
**Estimate:** 2–3.5 days

The website is a full-screen interactive Shuffle desktop demonstration, not a
traditional static product page. Reuse the suite's visual assets and interaction
logic where practical while implementing a web-native, maintainable presentation.

### Website A — desktop shell and guided entry

**Estimate:** 0.5–0.75 day

- [ ] Full-screen desktop composition based on Shuffle
- [ ] Responsive tablet and desktop layout
- [ ] Active guided information for first-time visitors
- [ ] Clear path to enter, skip or replay guidance
- [ ] Ghost White, spacing, surfaces, typography and motion from the design kit

### Website B — Shuffle capability cards

**Estimate:** 0.75–1 day

- [ ] Present the product-facing Shuffle capabilities while crediting Kadunce,
  Tettegouche and Temperance as the open-source component foundation.
- [ ] Clicking a card makes it Active.
- [ ] Active card expands into a concise feature/integration page.
- [ ] Moving between cards communicates how the suite fits together.
- [ ] Preserve the distinction between spatial windows, ongoing context and events.

### Website C — interactive demonstrations

**Estimate:** 0.75–1.5 days

- [ ] Recreate representative Card Line/Bento interactions.
- [ ] Demonstrate responsive Ambient media and transfer behavior.
- [ ] Demonstrate Temperance ticker, notification and system surfaces.
- [ ] Port existing visuals/assets where licensing and web rendering allow.
- [ ] Use constrained simulations rather than embedding product implementation.
- [ ] Keep motion interruptible and respect reduced-motion preferences.

### Website D — release surface

**Estimate:** 0.5 day

- [ ] Supported-environment and installation guidance
- [ ] Download/repository links and checksums
- [ ] Upgrade, rollback and uninstall instructions
- [ ] Release notes and known limitations
- [ ] Accessible keyboard/touch interaction
- [ ] Performance, responsive-layout and link smoke tests
- [ ] Production deployment to the Mac mini

### Exit gate

A visitor can understand Shuffle and its open-source component foundation, interact
with representative product behavior, explore focused capability cards, and reach
a verified consumer installation path. J gives final desktop and tablet acceptance
before publication.

## Rolling execution schedule

Exact working days will be replanned after Table and Keyboard feasibility. Preserve
this dependency order:

1. Finish Debug/Polish and remaining component features.
2. Complete the component refactor/lifecycle audit.
3. Prove and implement Table.
4. Prove and implement Shuffle Keyboard.
5. Establish the Good Input organization and private Shuffle consumer product.
6. Complete consumer packaging, clean installation, upgrade and rollback acceptance.
7. Configure the Mac mini host and tablet administration.
8. Build, deploy and accept the interactive Shuffle website and release surface.

Table and Keyboard research may overlap when it does not compete for the same
engineering owner or physical test loop. Website and server work stay at the bottom
of the roadmap and do not pull resources from product correctness.

## Progress update rule

At the completion of each numbered section:

1. Change its status to **Complete** and check accepted items.
2. Record actual engineering time, J testing time and approximate weekly usage.
3. Update source/install/push hashes in `CURRENT_STATE.md`.
4. Recalculate the remaining estimates using the newly observed delivery rate.
5. Adjust the planning number upward or downward rather than preserving an obsolete
   estimate.

Statuses are **Queued → In progress → Physical review → Complete**. A blocker gets
its own explicit note, owner and decision needed from J.
