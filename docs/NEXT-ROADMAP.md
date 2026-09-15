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

## Active work block — Tette Files and Ambient

Finish these as two bounded, independently reviewable packets. They may proceed
in parallel in isolated worktrees; integrate only after focused tests and review.

### T1 — Files operation plumbing

Owner: A-Team/Astra.

- Add useful operation progress and explicit cancellation where KIO supports it.
- Make collision, interruption, dismissal, guest loss, application launch and
  process-exit behavior truthful and recoverable. Preserve the existing bounded
  process-owned job lifetime; no always-on service without new evidence.
- Use disposable fixtures. Preserve partial-success reporting and refuse unsafe
  overwrite behavior unless a deliberate conflict decision is implemented.
- Do not include thumbnails, recursive search, devices/network, external drag,
  Open With, properties or unrelated Files polish.

Exit: focused native/QML tests and local build pass; source behavior and remaining
limits are documented; J receives a short physical copy/move/cancel/collision and
dismiss/reopen checklist. No install or push before review.

### T2 — Ambient Tette

Owner: B-Team/Sol.

- Keep full Apps/Files controls on the Active-sized surface. Leaving content mode
  returns to the quiet compact Tette card while preserving query, file location,
  history, selection, scroll and appropriate focus.
- Preserve Escape precedence, Meta dismissal, guest cleanup, drawer motion,
  standalone fallback and accepted Kadunce ownership.
- Track expansion ownership where the current protocol supports it. If manual
  persistent Active cannot be distinguished from content-caused expansion,
  implement only the unambiguous automatic contraction and surface the product
  decision instead of inventing a protocol.
- No normal-window/Bento conversion, new service, redesign or speculative
  Kadunce contract.

Exit: focused QML/controller tests and local build pass; J receives an Apps/Files
expand, exit, state-retention, Meta and standalone physical checklist.

Stop rule for both packets: two unsuccessful candidates or a required new owner/
cross-repo protocol triggers evidence review and a fresh scope decision.

## Next major feature blocks

Sequence is product-first; independent work may overlap only when files and
acceptance surfaces are disjoint.

1. **Tette Files completion:** transfer robustness acceptance, then desktop
   integration (external drag/drop, Open With, properties), discovery
   (thumbnails/previews, recursive search, Recent), and storage lifecycle
   (removable devices and optional KIO/network places). Keep these as related
   2–3 feature packets rather than one file-manager rewrite.
2. **Tette desktop/Bento use:** verify standalone display, focus and launch
   destination before choosing any guest-contract expansion.
3. **Steam/external libraries:** scoped discovery and launch. Steam owns library
   and Proton resolution; missing drives fail safely without boot mounts.
4. **Temperance release closure:** recheck current source before assigning the
   truthful power-state audit, stock-notification presenter handback or remaining
   badge details. Historical inventory is not proof of a current bug.
5. **Suite integration and packaging:** build each repo from clean source; verify
   install/uninstall or rollback paths, Kadunce persistent safety control,
   tablet-only and docked behavior, scaling/rotation, focus/Meta cleanup and
   package provenance. J performs final physical acceptance for each release.

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
