# Itasca launch roadmap — rolling execution plan

Updated September 15, 2026. This is the active roadmap and status format for the
remainder of Itasca 1.0. Update each section as work is accepted. After every
meaningful block, record actual elapsed time and revise the remaining planning
number from observed delivery speed.

J owns product priority and physical acceptance. B-Team/Sol is the default bounded
implementation owner. A-Team/Astra owns cross-suite architecture, Kadunce, risky
lifecycle work and ownership gates. Repo source remains authoritative.

## Planning snapshot

- **Current planning number:** 6 focused working days
- **Expected launch range:** 5–7 working days
- **Aggressive path:** 4 days if regressions are narrow and the website ports cleanly
- **Review point:** after Debug/Polish and again after the first interactive website proof
- **Budget target:** roughly 70–110% of one additional weekly allowance; stop and
  review before materially exceeding that range

Today established a much faster baseline than the original estimate: the major
Ambient architecture, real providers, design system, physical corrections and
suite freeze landed in roughly six hours at about 20% weekly usage. The planning
number is therefore a rolling forecast, not a fixed promise.

## 1. Debug and polish

**Status:** In progress
**Estimate:** 1–1.5 days
**Authoritative checklist:** `POST-FREEZE-TEST-20260915.md`

### A-Team — behavioral regressions

- [x] Cross-display Escape release returns the window to the tablet ownership origin.
- [x] Windows commit to Card Line ownership on entry without first becoming Active.
- [ ] Temperance ticker is no longer confined to the arrow-control view box.
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
- [ ] `do not disturb` is lowercase.
- [ ] Power glyphs are slightly smaller.
- [ ] Popup-to-dock clearance matches the accepted external spacing.
- [ ] Control Center and System Tray follow the design-kit spacing hierarchy.
- [ ] Drawer reveal, dropdown bounds, sorting, hover/active pills and Files visuals
  align with the accepted kit.

### Completion handoff

Tette owns the brief authoritative completion state while an activity finishes.
Temperance may then surface the completed transition when it remains useful or
actionable. Do not leave duplicate persistent completion on both sides.

### Exit gate

A1 retained-ownership correction passed J's physical tests for safety control,
cross-display release and immediate Card Line ownership. A2 Card Line/Bento
ownership is now the gate before further Tette feature iteration. One installed
candidate must pass tablet-only and docked-monitor testing. Any missing window,
stuck input, or broken Kadunce disable control blocks release immediately.

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

## 3. Refactor audit and consumer install package

**Status:** Queued
**Estimate:** 1–1.5 days

### Refactor audit

- [ ] Remove accidental duplication introduced during the sprint.
- [ ] Verify Tette/Temperance activity ownership and cleanup boundaries.
- [ ] Audit timers, model lifetime, responsive calculations and resource paths.
- [ ] Confirm protected custom icons remain intact.
- [ ] Review Kadunce animation scaling/reduced-motion behavior.
- [ ] Restrict fixes to visible reliability, maintenance or release risk.

This is a surgical audit, not a rewrite.

### Consumer installer

- [ ] Clean builds from all three repositories
- [ ] One Fish-safe suite installation path
- [ ] Plasma restart/session instructions included in the completed flow
- [ ] Dependency detection with useful failures
- [ ] Version and package provenance
- [ ] Upgrade behavior
- [ ] Rollback and uninstall
- [ ] Persistent Kadunce safety-control verification
- [ ] Fresh-user installation test

The launch package targets the supported KDE Plasma Linux environment. A universal
multi-distribution matrix is outside initial release scope.

### Exit gate

A fresh supported machine can install, update, roll back and uninstall without
repo knowledge. Kadunce's live disable control remains functional throughout.

## 4. Mac mini website host and tablet control

**Status:** Queued
**Estimate:** 0.25–0.5 day, excluding router/DNS surprises

The Mac mini only hosts the Itasca website and its repository. J's personal site
may move there later, but that migration does not block Itasca launch.

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

## 5. Interactive website and release

**Status:** Queued
**Estimate:** 2–3.5 days

The website is a full-screen interactive Itasca desktop demonstration, not a
traditional static product page. Reuse the suite's visual assets and interaction
logic where practical while implementing a web-native, maintainable presentation.

### Website A — desktop shell and guided entry

**Estimate:** 0.5–0.75 day

- [ ] Full-screen desktop composition based on Itasca
- [ ] Responsive tablet and desktop layout
- [ ] Active guided information for first-time visitors
- [ ] Clear path to enter, skip or replay guidance
- [ ] Ghost White, spacing, surfaces, typography and motion from the design kit

### Website B — Kadunce, Tettegouche and Temperance cards

**Estimate:** 0.75–1 day

- [ ] Present Kadunce, Tettegouche and Temperance as the three primary cards.
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

A visitor can understand the three products, interact with representative suite
behavior, open each product card for a focused explanation, and reach a verified
consumer installation path. J gives final desktop and tablet acceptance before
publication.

## Rolling execution schedule

| Working day | A-Team | B-Team | PM / J |
|---|---|---|---|
| 1 | Kadunce ownership and ticker gates | Ambient, notifications and visual polish | Installed acceptance loops |
| 2 | Card manipulation and event contract | Files B and Files C | Resolve product edges; start website story |
| 3 | Refactor/lifecycle audit | Event UI and remaining feature slice | Website shell and guided entry |
| 4 | Consumer installer | Package corrections | Product cards and interactive demos |
| 5 | Release/safety verification | Website interaction support | Mac mini deployment and website acceptance |
| 6 | Buffer and launch corrections | Buffer and launch corrections | Final release |

Website work can begin after the Debug/Polish interaction behavior is stable enough
to serve as the demonstration source. Server setup can overlap the website build.

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

## Post-1.0 — Kadunce Table 1.1

Table remains post-launch product direction:

`Active = this window → Card Line/Bento = these windows → Table = these workspaces`

KWin/Plasma remain authoritative for virtual desktops. Gesture ownership,
multi-display behavior and the KDE virtual-desktop API require explicit audits
before implementation. See `KADUNCE-TABLE-1.1-CONCEPT.md`.
