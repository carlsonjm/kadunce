# Itasca launch roadmap — September 15, 2026

This is the active suite queue. It replaces the September 13 R1-first order and
the historical P0–P3/K0–K6 directives. J owns product priority and physical
acceptance. Repo source and current-state documents remain authoritative for
implementation and installed provenance.

## Accepted baseline — preserve

- Kadunce native touch carry, Card Line/Bento operation, edge entry, dock-safe
  exit, output handoff, Active ownership lifetime, shared paging and stack
  browsing are accepted. September 14 monitor admission and tablet ownership
  corrections close the majority of the former R1 block.
- Tette Meta toggle, fullscreen panel access, shared Active drawer sizing,
  dock-safe bounds, Browse Everything, and the accepted local Files foundation
  are in main. Files includes navigation, selection, internal copy drag/drop,
  copy/paste, create, rename, cut/move, recoverable Trash and default-app open.
- Temperance notification readability, banner behavior, Bluetooth pairing and
  aligned controls are accepted.
- Accepted behavior is a regression contract. Do not rebuild it to address an
  unverified historical report.

## Active work block — Ambient Tette

Files basics are ready enough to leave the critical path. The local
transfer-robustness candidate remains available for later review and promotion;
do not expand its test matrix before current product work.

Ambient Tette is the right-side dock surface for ongoing context: **what matters
now**. Temperance remains the left-side notification ticker today and owns the
future transient-event surface for **what changed**. Source apps and services own
actual state and actions. There is no separate Live Rail app.

Width reveals information, not capability. Ambient packs concurrent activity:
each item keeps a minimum recognizable state and essential action while extra
space reveals filename/title, artist, bytes, duration, and ETA. A transfer may
compress media description, but must not remove media controls when both cores
fit. Similar activities group only when their cores cannot all fit. Both flexible
sides preserve the physical center of the application dock.

Authoritative contract and packet gates:
`../../tettegouche/docs/AMBIENT-LIVE-ACTIVITY-HANDOFF.md`.

1. A-Team: read-only source/geometry feasibility and exact provider contract.
2. B-Team: responsive compositor against that approved contract.
3. A-Team: production providers/integration after J accepts the interaction.
4. B-Team: later Temperance event-source expansion; current Temperance is
   notification-only and does not block initial Ambient.

No daemon, polling loop, persistent history, replacement task manager, or broad
Plasma patch without a new architecture/budget decision. Two failed approaches
to one boundary trigger evidence review.

## Next major feature blocks

Sequence is product-first; independent work may overlap only when files and
acceptance surfaces are disjoint.

1. **Ambient Tette:** complete its gated source, compositor, and integration
   packets, then J's tablet/monitor acceptance.
2. **Tette Files completion:** optional transfer-robustness promotion, then desktop
   integration (external drag/drop, Open With, properties), discovery
   (thumbnails/previews, recursive search, Recent), and storage lifecycle
   (removable devices and optional KIO/network places). Keep these as related
   2–3 feature packets rather than one file-manager rewrite.
3. **Tette desktop/Bento use:** verify standalone display, focus and launch
   destination before choosing any guest-contract expansion.
4. **Steam/external libraries:** scoped discovery and launch. Steam owns library
   and Proton resolution; missing drives fail safely without boot mounts.
5. **Temperance release closure:** add the bounded non-notification event-source
   slice after initial Ambient, then recheck current source before assigning the
   truthful power-state audit, stock-notification presenter handback or remaining
   badge details. Historical inventory is not proof of a current bug.
6. **Suite integration and packaging:** build each repo from clean source; verify
   install/uninstall or rollback paths, Kadunce persistent safety control,
   tablet-only and docked behavior, scaling/rotation, focus/Meta cleanup and
   package provenance. J performs final physical acceptance for each release.

## Post-1.0 product direction — Kadunce 1.1 Table

J has defined **Table** as the working concept for Kadunce 1.1. It extends the
existing spatial hierarchy outward without replacing KDE Virtual Desktops:

`Active = this window → Card Line/Bento = these windows → Table = these workspaces`

The first acceptance proof is intentionally complete and narrow: from Card Line
on a touch device, four-finger swipe up into Table, show multiple existing KDE
Virtual Desktops as spatial surfaces, drag one real Kadunce-managed window to a
different desktop, enter that desktop, and find the window in its Card Line while
KDE still reports correct underlying membership.

KWin/Plasma remain authoritative for virtual-desktop identity, membership,
switching, lifecycle and persistence. Table must reuse Kadunce's accepted card,
ownership, display and transfer architecture. Virtual desktops and physical
displays remain separate dimensions. Multi-display behavior and gesture ownership
require explicit design/engineering audits before implementation; the proposed
four-finger horizontal gesture is not accepted scope.

Table is explicitly outside Itasca 1.0, the current installer/website path and
release-critical work. Engineering begins only after 1.0 is stable enough to
reopen Kadunce feature development. Full product brief and non-goals:
`KADUNCE-TABLE-1.1-CONCEPT.md`.

## Debug block — after major feature work

Collect remaining observed defects here and reproduce each on current main before
editing. The former R1 ownership/drop-target reports are no longer the active
queue; September 14 cleared most of that block. Candidate items include only
failures that still reproduce:

- card/Bento ownership, spring-back or preview-versus-drop-target mismatch;
- Ghostty's reported stray one-pixel bottom line/flat-dock trigger;
- Zen rendering/sampling artifact;
- Affinity splash/main-window transition;
- cross-display arrival discontinuity or remaining Bento reflow motion;
- any Tette/Temperance regressions discovered during feature acceptance.

For each item capture one failing route and one neighboring passing route, find
the first divergent owner/decision, and make the smallest correction. Two failed
approaches require evidence review. Any lost window, stuck input or broken
Kadunce disable control remains an immediate safety blocker and does not wait for
this block.

## Launch definition

Itasca is launch-ready when the selected V1 feature set is accepted, all three
repos build from clean source, packages and rollback/uninstall paths are verified,
Kadunce's persistent kill switch passes live control checks, no release-blocking
data-loss/input/window-loss issue remains, and J completes final tablet-only and
docked acceptance. Broader hardware coverage, persistent layouts, renderer
extraction, new tiling modes and new always-on services remain post-launch unless
J explicitly promotes them.

## Resource and budget policy

- B-Team/Sol is the default bounded implementation owner. A-Team/Astra takes
  cross-owner architecture, risky lifecycle work and review gates.
- Assign one coherent packet at a time with a visible failure or outcome, exact
  owner, focused tests, physical acceptance and stop point.
- Prefer focused checks over broad campaigns. Do not rebuild unchanged packages.
- Architecture cleanup alone does not block a usable release. Scope expansion,
  a new persistent owner or a broad compatibility matrix requires a product and
  budget decision from J.
