# Shuffle execution plan

This plan orders work by dependency and by what recent evidence changed. It is
the only execution plan in this repository. It absorbed the superseded
`NEXT-ROADMAP.md`, whose product boundary, execution policy, planning controls
and still-open task detail are carried below; Git holds that file's history.

## Product boundary

- Good Input publishes Shuffle for Plasma.
- Kadunce, Tettegouche and Temperance remain open-source component repositories.
- A private Shuffle repository owns consumer assembly, private assets, release
  packaging, product naming and support material.
- Table and Shuffle Keyboard are required 1.0 capabilities.
- KWin, Plasma and the system input stack remain authoritative for windows,
  virtual desktops and text delivery.

## Execution policy

- J approves product behavior and visual direction before implementation begins.
  Engineering may present evidence, constraints and alternatives, but an
  unapproved proposal does not become a candidate.
- One implementation owner per repository. A second worker is read-only review or
  a disjoint file set.
- Identify and measure the property controlling a defect before changing it.
- Reproduce a defect before fixing it. A fix whose symptom cannot be reproduced is
  not yet a fix.
- Batch two or three related physical checks into one candidate. Freeze passed
  items; later candidates touch only failures.
- Separate feasibility, minimal prototype and product implementation.
- Keep rejected candidates out of `main`; preserve useful evidence in `archive/`.
- Update durable documentation only for accepted or rejected behavior, an
  architecture or scope change, a sequencing blocker, installed provenance, or a
  published freeze.

## Planning and budget controls

Two production samples from 17 September 2026 set the planning baseline:

- The accepted Bento group-card block used roughly four hours and 1,260 purchased
  credits ($50.40 at 2,500 credits per $100) for one primary architecture solve,
  required scenario logic, one unapproved false start, one provisional acceptance,
  and several physical adjustments.
- The full production day used 2,421 purchased-credit equivalents ($96.84) across
  implementation, physical iteration, ownership investigation, freezes, and
  documentation. This is approximately half of one observed weekly included limit.

For scheduling only, treat one weekly included limit as roughly 5,000 purchased
credits or $200 of purchased-credit capacity. Four weekly limits are therefore
roughly $800 of monthly purchased-credit capacity. This is an empirical planning
proxy, not a billing guarantee.

- Schedule every major architecture change or architecture-level bug fix inside the
  weekly included-limit plan. Reserve one focused weekly block before starting; do
  not fund exploratory architecture with purchased credits or a small residual
  weekly balance.
- Use an approved interaction model and explicit physical acceptance checks as the
  entry gate. If the model is undecided, spend only on evidence and alternatives.
- Budget a comparable architecture solve at no less than 4-6 focused hours and one
  protected weekly block until later accepted work provides a better baseline.
- Purchased credits may flex for bounded procedural work such as documentation,
  packaging, installer corrections, mechanical cleanup, or a well-isolated fix with
  a known controlling property and stop condition.
- Give credit-funded work a fixed packet, acceptance test, and spend ceiling. Stop
  when evidence changes the task into architecture or product design.
- A budget estimate schedules the work; it does not justify cutting off an agent
  before a coherent implementation or safely preserved candidate exists.
- Preserve unfinished but useful architecture candidates on named WIP branches.
  Promote only physically accepted behavior to `main`.
- Re-estimate this baseline after each accepted major slice using elapsed time,
  consumed credits, false starts, and physical candidate count.

## Working model

Two session types share GitHub and have different reach.

| | Local session | Cloud session |
| --- | --- | --- |
| Full CTest, KWin-linked and D-Bus tests | yes | no |
| Package, control, install, physical review | yes | no |
| `tests/verify-headless.sh` domain suite | yes | yes |
| Reading, analysis, documentation | yes | yes |

Domain-layer work in `CardWorkspaceState`, `SpreadModel`, `SpreadLayout` and
`BentoLayout` is fully covered by the headless suite and can be prepared in either
session. Anything touching `Effect`, `WorkspaceInputRouter`, packaging or the
installed system requires a local session. A headless pass is a pre-check, never
promotion evidence.

## Ordering rationale

Four findings changed the order. They are recorded in
`OWNERSHIP-AUDIT-20260919.md`.

1. Ownership has no single representation. Individual cards live in
   `CardWorkspaceState`, Bento panes in `DesktopStageController` sessions, and
   Native only as absence from both. No object can evaluate
   `CARD-LIFECYCLE.md` §14's first invariant, so every transition maintains two
   containers by hand. This is why the ownership block has not closed.
2. Prepared transactions copy and write back the whole `SpreadModel`, which
   carries selection and paging. Presentation state travels inside ownership
   tickets, so the revision guard that voids them is load-bearing and cannot be
   removed first.
3. `tests/verify-source.sh` asserts the position of strings inside implementation
   files. Six of those assertions name symbols the ownership work rewrites. They
   reject correct code and must be converted before, not during, that work.
4. The Shuffle dock is infrastructure, not packaging. It resolves Tettegouche's
   asymmetric Ambient and ticker width and supplies the Keyboard's mount
   geometry, so it gates two later blocks and cannot sit inside private assembly.

Table and Keyboard feasibility remain after the component engines meet their
contracts. Proving input plumbing against a shell that does not yet satisfy
`CARD-LIFECYCLE.md` would prove it against a moving target.

## Block 1 — Refactor enablement

**Status:** Complete. Block 2 is unblocked.

### 1d. Make an installed candidate actually run

**Status:** Ready. Found while validating Block 2 live.

- [ ] `install.sh` finishes with the effect unloaded: its unload/re-enable and
  `reconfigure` do not reload it, so the workspace is left without Kadunce until
  something loads it. Reload it explicitly and verify the object is back.
- [ ] KWin keeps the previous plugin image mapped across an unload, so loading
  after an install can re-instantiate the previous build. Detect that and say so
  rather than leaving the installer's advice to restart Plasma as the only hint.

### 1a. Free the checks from implementation shape

**Status:** Complete. No check now asserts the position of an ownership symbol.

- [x] Convert the six source-order assertions in `tests/verify-source.sh` to
  behavioral coverage of the same invariants: destination acceptance before
  source removal, publication before native placement, projection retirement
  before resume commits. The three invariants are now duck-typed sequences in
  `OwnershipHandoff.h`, driven by the headless `ownership-handoff` test.
- [x] Keep the packaging, service-name and plugin-identity greps; they assert
  installed facts rather than implementation shape.

### 1b. Apply the terminology contract

**Status:** Complete for Kadunce. Tettegouche carries two retired consumer
strings and neither it nor Temperance has the guard yet.

`TERMINOLOGY.md` holds the approved and retired language and the three-layer
rule. This step applies layers 1 and 2 only; layer 3, package and interface
identity, is Block 10b.

This runs after 1a because the current checks match symbol text, and before
Block 2 because Blocks 2 and 3 rewrite exactly the code that carries the retired
name. Renaming afterwards would touch those lines twice and would let new
ownership code be written in retired vocabulary.

Measured scope for the retired term: 248 occurrences across 27 Kadunce source
files, 6 filenames, 15 live Kadunce documents and 4 Tettegouche documents. No
Tettegouche or Temperance source depends on it, though two Tettegouche consumer
strings carry it.

- [x] Rename code symbols, filenames and live documentation in one mechanical
  commit with no behavior change, reviewable and bisectable on its own.
- [x] Confirm consumer-facing text uses Workspace, Spread, Search, Status Bar and
  Ambient as the contract defines them. Kadunce ships no consumer product term:
  `Good Input` and `Shuffle*` appear in documentation only, never in source,
  packaging or a user-visible string.
- [x] Leave `docs/archive/` unchanged.
- [x] Leave every identifier in layer 3 unchanged, including the `Q_SCRIPTABLE`
  `showCardLine` method, the `cardLine` context value and the persisted
  `Kadunce Card Line` global-shortcut identity. All three are on the installed
  surface; retiring them is a versioned protocol change, not a vocabulary pass.
- [x] Extend the retired-identity guard from `tettegouche/tests/verify-source.sh`
  to Kadunce, then add retired vocabulary to its pattern so the rename cannot
  regress. Temperance and Tettegouche still need the same guard.

### 1c. Retire duplicated invariant text

**Status:** Complete.

- [x] Reduce the duplicated invariant text across `ARCHITECTURE.md`,
  `DECISIONS.md`, `PRODUCT-CONTRACT.md` and `CURRENT_STATE.md` to one owner per
  invariant, so a later ownership change cannot leave four documents disagreeing.
  `docs/README.md` § One owner per invariant records which document owns what and
  states that the owner governs a conflict.
- [x] Decide whether `NEXT-ROADMAP.md` is retired into this file or kept as
  component task detail, and make the index say which. Retired: it was a second
  complete plan with its own block numbering, not task detail, and the two had
  already diverged. Its unique planning controls and audit items moved here.

**Exit gate:** The full suite passes, no check fails purely because a symbol
moved, and no live document or source symbol carries the retired workspace term
outside the three frozen layer-3 identities.

## Block 2 — Ownership foundation

**Status:** Complete. Block 3 is unblocked.

- [x] Narrow prepared tickets to the membership, order and grouping delta they
  intend. A ticket must not transport selection, page offset or neighbor side.
  Preparation now proves the delta against a throwaway model and keeps only the
  intent; commit re-derives the result from the live model.
- [x] Separate ownership and presentation revisions once tickets no longer carry
  presentation. `revision()` keeps its current meaning for preview and
  carry-provenance owners; transactions bind to the ownership revision.
  Admission and removal bind to `ownershipRevision()`, which is what Finding 2
  reproduced. Stack insertion still binds to `revision()`: its delta names a
  visible stack face and the selected source card, so paging changes what it
  means rather than merely when it was issued.
- [x] Introduce the ownership value as an assertion-only observer across both
  stage controllers, recording Native, individual card and Bento pane without
  changing behavior. Every §14 violation it reports is a pre-existing defect.
  `CardOwnership.h` holds the value and the audit; `DesktopStageController`
  exposes a read-only view of each session's pane and overflow identities, and
  `Effect` runs the audit on every published workspace change, reporting a
  shape once so a standing defect cannot bury the next new one.
- [x] Make it authoritative. `CardOwnershipLedger` is the authority for who owns
  a window: a stage's observed owner is put to it as a transition, it accepts
  only the six the contract defines, and it reports every change it refuses and
  every container that disagrees with it. §14 is evaluated in that one place.
- [x] Collapse the prepared types onto the six directed transitions, as far as
  that premise holds. Only two of the five are ownership transitions, and they
  now name which of the six they perform. `PreparedStackInsertion` is grouping
  inside card ownership and changes no owner; `PreparedDrop` carries destination
  geometry and drop intent; `PreparedCarrySource` is read-only source provenance
  that removes no membership. Folding those three into ownership transitions
  would put placement and provenance back inside ownership tickets, which is the
  conflation Finding 3 removed in the other direction. The remaining `prepare*`
  surface spans card membership, Bento session planning, drop placement and
  carry provenance; consolidating the Bento half belongs to Block 3, which
  rewrites those paths for visible-pane ownership.

**Exit gate:** A held reservation survives ordinary paging and selection; a
membership change still voids it; §14 is enforced in one place; headless and full
suites pass.

## Block 3 — Ownership behavior

**Status:** Blocked by Block 2. This is the behavior the previous plan's first
block requested, now expressible.

- [ ] Bento owns only its visible pane combination; minimized, displaced,
  overflowed and extracted windows become independent cards with no retained
  association. Measured: a five-window tablet Bento yields two panes and three
  overflow windows owned by nobody, which is three §14 violations and the
  defect this item removes.
- [ ] First Card or Bento entry atomically adopts every eligible window on that
  display and current virtual desktop.
- [ ] Atomic prepared admission and removal for one logical group: selecting a
  group transfers only that group; selecting an individual preserves the group.
- [ ] Rebuild top-edge Active extraction on that contract, covering both
  selection paths, rollback, repeated transitions, release, unload and
  other-output isolation.

**Exit gate:** Automated ownership coverage plus physical two-pane, three-pane,
repeated-selection, cold-start and multi-display checks pass. No missing window,
stuck input, broken restoration, cross-output leak, or failed disable control.

## Block 4 — Kadunce manipulation

**Status:** Blocked by Block 3.

- [ ] Make stack extraction reliable; native-to-stack arrival becomes one atomic
  membership and insertion transaction.
- [ ] Give reordering a usable intent zone without accidental paging.
- [ ] Complete arrival, displacement, cancellation and neighbor motion.
- [ ] Make custom compositor motion follow platform animation scaling and
  reduced-motion preferences.
- [x] Normalize live terminology to Spread. Completed in Block 1b.

**Exit gate:** Physical review accepts manipulation on supported hardware, and
user-facing terminology matches the contract.

## Block 5 — Bottom Surface

**Status:** Parallel with Blocks 1–4; different repository and disjoint physical
checks. Requires Block 10a first.

Bottom Surface is the single layout authority for the Status Bar, Shuffle Dock,
Ambient and the Keyboard boundary. Treating it as one authority is the point:
today those surfaces negotiate width independently, which is why Ambient and the
ticker sit asymmetrically against a third-party dock.

It is a paid product feature and lives in the private Shuffle repository.
Tettegouche and Temperance must remain fully functional without it: the dock
optimizes their composition, and is never a dependency of it. An open-source
installation that lacks the dock is a supported configuration, not a degraded
one.

- [ ] Complete Block 10a so the private repository exists to hold it.
- [ ] Author the Bottom Surface contract: reserved geometry, work area, and what
  Kadunce's dock clearance and Tettegouche's responsive composition consume.
- [ ] Implement Shuffle Dock as minimal task and application presentation inside
  that surface.
- [ ] Resolve the asymmetric Ambient and ticker width.
- [ ] Define the Keyboard boundary in the same contract, so Block 9 inherits it
  rather than negotiating it.

**Exit gate:** Status Bar, Ambient and Shuffle Dock are physically symmetric at
tablet and monitor widths, and the Keyboard boundary is specified without a
keyboard existing.

## Block 6 — Tettegouche completion

**Status:** Width work blocked by Block 5; the rest is ready.

- [ ] Ambient release validation against `AMBIENT-CONTRACT.md`: MPRIS, Plasma
  jobs, Tette operations, Downloads arrivals, concurrent density.
- [ ] Media metadata priority and dock-aware width recomputation.
- [ ] Transfer pause, authoritative completion, conflict choices preserving the
  no-silent-overwrite guarantee.
- [ ] Files properties, previews, recursive search, Recent, and bounded error
  states as separately accepted packets.
- [ ] Storage lifecycle without weakening KIO ownership or truthful reporting.

**Exit gate:** Each slice has focused automated checks and an installed
acceptance pass. One provider failure does not block unrelated slices.

## Block 6b — Maintainability and lifecycle audit

**Status:** Ready. Absorbed from the retired `NEXT-ROADMAP.md`. Bounded on
purpose: it does not reopen stable architecture for style.

- [ ] Remove accidental duplication that creates visible reliability or
  maintenance risk.
- [ ] Verify Tettegouche and Temperance activity ownership and cleanup
  boundaries.
- [ ] Audit timers, model lifetime, responsive calculations and resource paths.
- [ ] Confirm protected custom icons remain unchanged.

**Exit gate:** The three components are lifecycle-safe and ready for private
product consumption without duplicated state ownership.

## Block 7 — Temperance event boundary

**Status:** Ready. Smallest remaining component block.

- [ ] Add only authoritative, useful system transitions with defined identity,
  freshness, priority, deduplication and dismissal.
- [ ] Preserve Tettegouche ownership of live progress and actions; neither side
  retains the same completion indefinitely.
- [ ] Revisit presenter switching only after its process-wide lifecycle is proven.
- [ ] Refine action-pill typography or horizontal padding.

**Exit gate:** Events are authoritative and deduplicated; no duplicate ownership
of a completion.

## Block 8 — Table feasibility and implementation

**Status:** Blocked by Blocks 3 and 4. Reference:
`KADUNCE-TABLE-1.1-CONCEPT.md`.

- [ ] Audit KWin and Plasma virtual-desktop APIs, gesture ownership and lifecycle.
- [ ] Prove multi-display behavior without changing output-local Bento semantics.
- [ ] Build the smallest complete prototype and test it physically.
- [ ] Implement the accepted interaction only after feasibility passes.

**Exit gate:** A user enters Table, moves a real managed window between existing
KDE virtual desktops, enters the destination Spread, and observes correct desktop
membership without regressing normal KDE switching or display ownership.

## Block 9 — Shuffle Keyboard feasibility and implementation

**Status:** Blocked by Blocks 4, 5 and 8. Highest residual product risk.
Reference: `SHUFFLE-KEYBOARD-1.0-CONCEPT.md`.

- [ ] Audit Plasma Keyboard, Qt Virtual Keyboard, KWin input-method plumbing and
  Fcitx5. Select a system-backed base or document why none is viable.
- [ ] Prove the four-row layout, cascading controls, adjustable height, and the
  keyboard to precision-surface transition against the Block 5 dock geometry.
- [ ] Verify locale and keymap correctness, focus, latency and loss-free input
  across Qt/KDE, GTK, browsers, Electron and terminals.
- [ ] Reserve usable workspace correctly as height changes.
- [ ] Keep autocorrect, prediction, swipe typing, dictation and custom IME work
  out of 1.0.

**Exit gate:** A supported 10–13 inch touch device performs reliable text and
precision input without physical peripherals. Dropped characters, meaningful
latency, wrong keymaps, focus loss or unreliable show and hide block release.

If no system-backed base proves viable, this block returns a scope decision to J
rather than a custom input engine.

## Block 9b — Shuffle Lock

**Status:** Blocked by Block 9. No contract, concept document or implementation
exists anywhere in the three repositories; the term appears in zero files. This
is the largest undocumented 1.0 commitment.

Shuffle Lock is a privacy-first presentation over the trusted system lock and
authentication. It depends on the Keyboard, because authenticating on a tablet
without physical peripherals requires it.

- [ ] Author the product contract: what is concealed, what is shown, and what
  the user can do before authenticating.
- [ ] Confirm KDE's screen locker remains the authentication authority. Shuffle
  presents; it never handles credentials or replaces the lock.
- [ ] Prove the Keyboard is available and correct on the lock surface.
- [ ] Define behavior on failure, interruption, timeout and multiple displays.

**Exit gate:** The lock surface conceals workspace content, authenticates
through the system locker, accepts Keyboard input reliably, and a failure in
Shuffle presentation never leaves a session unlocked or unrecoverable.

## Block 10a — Private repository

**Status:** Ready. Pulled ahead of the rest of Block 10 because Block 5 lives
here.

- [ ] Establish the Good Input organization and private Shuffle repository.
- [ ] Document the public component and private product boundary, including the
  rule that components never depend on private features.

## Block 10 — Private assembly and installation

**Status:** Blocked by Blocks 9 and 10a. Requires the compatibility decision
below.
- [ ] Consume pinned component versions with provenance; give the product a
  version identity distinct from component versions.
- [ ] Provide one Fish-safe installer with dependency detection.
- [ ] Verify versioning, upgrade, rollback, uninstall and the Kadunce disable
  control throughout.
- [ ] Complete a fresh-machine installation test.

**Exit gate:** A fresh supported machine installs, updates, rolls back and
uninstalls Shuffle without repository knowledge, with the disable control
functional at every step.

## Block 10b — Package and interface identity

**Status:** Blocked by Block 10a. Coordinated across all three repositories.

Layer 3 of `TERMINOLOGY.md`. Two namespaces are in use, `studio.warbler.*` and
`io.github.carlsonjm.*`, neither reflecting the publisher, and Tettegouche
hard-codes `studio.warbler.Kadunce` in seven call sites.

- [ ] Choose the Good Input reverse-DNS namespace.
- [ ] Rename service, interface, plugin and desktop-entry identifiers together,
  including the `showCardLine` method, the `cardLine` context value and the
  persisted `Kadunce Card Line` global-shortcut identity.
- [ ] Bump the workspace-context schema and launcher-guest protocol versions, and
  update Tettegouche's supported versions in the same release.
- [ ] Document the migration. Temperance 1.1.0 already showed that a package
  identity change forces users to remove and re-add the widget.

**Exit gate:** One namespace across the suite, no retired vocabulary on any
public surface, and a clean install and upgrade path on a machine that has the
previous identities installed.

## Block 11 — Host and public site

**Status:** Blocked by Block 10. Unchanged in scope from the previous plan.

- [ ] Configure the standard HTTPS host, tablet administration, startup, backups
  and uptime visibility.
- [ ] Build the Shuffle desktop shell, guided entry and capability cards.
- [ ] Test keyboard, touch, responsive layout, performance and accessibility.

**Exit gate:** The public site accurately demonstrates the released product and
provides a complete supported installation path.

## Open product decisions

These block later work and are not engineering calls.

1. **Compatibility policy.** KDE publishes no stable KWin effect ABI, so a
   Plasma update can leave the installed plugin unloadable. The failure is
   already safe: KWin declines the plugin, the tray switch persists, and the
   desktop keeps working. What remains is how a paying user gets a working
   plugin back.

   The current guided repair rebuilds a retained source snapshot on the user's
   machine. That requires a full C++, Qt, KDE Frameworks and KWin development
   toolchain on every consumer installation, which is a developer workaround
   rather than a consumer mechanism.

   **Approved direction:** distribute the plugin as a package from a Good Input
   pacman repository, built per KWin release in CI, so the user receives a
   corrected plugin through ordinary system updates. One supported distribution
   makes this cheap and centralizes maintenance in one pipeline instead of making
   every user's machine a build environment.

   **Scheduled in Block 10**, not before. Until the consumer bundle exists there
   is no repository to publish to and no consumer to protect; the guided repair
   is adequate for development machines, where the toolchain is present anyway.

2. **Shuffle Lock scope.** Block 9b has no contract yet. What the lock surface
   conceals and what remains usable before authentication is a product decision
   that must precede its feasibility work.

## Progress update rule

For an accepted block, update only its checkbox or status, a changed invariant or
scope decision, the next blocked dependency, and an estimate that new evidence
makes defensible. Do not append sprint narration, worker summaries, candidate
hashes or repeated test logs.
