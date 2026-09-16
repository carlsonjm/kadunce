# Itasca launch roadmap — September 15, 2026 EOD

This is the active suite queue after J's installed whole-suite pass. It replaces
the earlier Ambient-first packet list. J owns product priority and physical
acceptance. Repo source and current-state documents remain authoritative for
implementation and installed provenance.

## Accepted launch baseline — preserve

- **Kadunce:** native touch carry, Card Line/Bento, edge entry, dock-safe exit,
  output handoff, Active ownership lifetime, monitor admission, shared paging and
  stack browsing. The visual audit aligns the custom tray and spatial hints while
  preserving card/fan/Bento geometry and the persistent safety control.
- **Tettegouche:** Meta/panel access, Files foundation, responsive Ambient media
  and transfer activity, real MPRIS/KJob providers, compact local activity
  popups, responsive information density, completion feedback, and Lucide action
  chrome. The center application dock remains visually centered.
- **Temperance:** notification ticker/history, producer actions, individual and
  group dismissal, Bluetooth/power/status controls, left-boundary measurement,
  Lucide action chrome, and protected custom Bell/Weather/tray identity.
- **Suite language:** Ghost White `#F8F8FF`; pills are controls; rounded boxes are
  information; labels use lowercase where they are labels; provider identity is
  preserved; motion is bounded and interruptible.

The ownership boundary remains:

- Temperance = transient awareness, **what changed**.
- Ambient Tette = ongoing context, **what matters now**.
- Source apps/services = actual state and actions.

There is no separate Live Rail product. Width reveals information rather than
adding capability. Under constraint, Ambient now follows J's explicit shedding
order: artist, song, then previous/next; play/pause is the final compact media
capability.

## Post-freeze physical findings

J's installed testing produced a concrete replacement checklist covering Kadunce
ownership/reordering, Ambient constraint and transfer behavior, the Temperance
ticker/notification geometry, and suite control spacing. It is the current defect
intake: `POST-FREEZE-TEST-20260915.md`.

The first engineering gate is reproduction/ownership triage for Kadunce's
cross-display Escape target, delayed Card Line ownership, and Temperance's ticker
being confined to the arrow-control box. These are behavioral regressions and
take priority over the visual-polish packet below.

## Active block 1 — Tette component polish

Use the screenshots from J's September 15 review as the acceptance source. Keep
this bounded to visible inconsistencies:

1. Treat `close drawer`, path, new-item and similar actions as controls with
   consistent pill/icon-button affordances.
2. Remove premature truncation such as `New f...` when the drawer has unused
   width. Use measured fit rather than fixed descriptive tiers.
3. Replace remaining raw Unicode action marks and align search-clear, sort,
   Everyday/Quiet mode, and navigation states with the pinned Lucide subset.
4. Correct constrained media shedding to J's latest order: artist, song, then
   previous/next; play/pause is the final compact capability. Transfers must
   trigger the same fit calculation rather than pushing media under the dock.
5. Preserve the Tette Dot, real provider identity, current responsive boundary,
   local popups and center-dock geometry.

Owner: B-Team by default. Stop after focused build/tests and J's tablet/monitor
visual pass. Do not turn this into a Files rewrite.

## Active block 2 — Temperance release closure

Split this into two evidence-driven packets:

1. **Popup clearance:** capture the settled popup, panel and available-screen
   geometry in the live session. Target the established 10 px breathing room only
   at the layer that owns the external placement. Plasma-owned placement is a
   valid boundary; do not simulate the gap with card padding or reduced content.
2. **Event sources:** add the smallest useful non-notification transient source
   slice so Temperance can represent system transitions beyond notifications.
   Keep events ephemeral and separate from Tette's persistent live activities.

Owner: A-Team for the geometry/ownership gate; B-Team for the bounded event UI
after the contract is approved. Preserve notification actions, custom Bell and
left-side responsive measurement.

## Active block 3 — Tette Files completion

Treat these as related but separate packets:

1. Desktop integration: external drag/drop, Open With and properties.
2. Discovery: thumbnails/previews, recursive search and Recent.
3. Storage lifecycle: removable devices and optional KIO/network places.

The filesystem provider may observe incoming files, but observation alone cannot
invent authoritative progress or success. KJob/source ownership remains the
truth for percentage, cancellation and completion.

Tette owns the brief authoritative completion state. Temperance may then surface
the transition as an event when it remains useful or actionable; avoid duplicate
persistent completion on both sides.

## Active block 4 — launch integration

Build all three repos from clean source. Verify package provenance, install and
uninstall or rollback paths, Kadunce's persistent kill switch, tablet-only and
docked behavior, scaling/rotation, focus and Meta cleanup, and the final website
or distribution path. J performs final physical acceptance.

Launch requires no release-blocking data loss, input loss or window loss. A
missing/broken Kadunce disable control blocks release immediately.

## Deferred Debug block

After major feature work, reproduce each report on current main before editing:

- Ghostty's stray one-pixel bottom line/flat-dock trigger;
- Zen rendering/sampling artifact;
- Affinity splash/main-window transition;
- cross-display arrival discontinuity or remaining Bento motion issue;
- regressions found during current Tette/Temperance acceptance.

Most former R1 ownership/drop reports were cleared September 14. Do not restore
them to the queue without a current reproduction. Capture one failing and one
neighboring passing route, then correct the first divergent owner/decision.

## Post-1.0 — Kadunce Table 1.1

Table extends the existing spatial hierarchy:

`Active = this window → Card Line/Bento = these windows → Table = these workspaces`

KWin/Plasma remain authoritative for virtual desktops. The first proof is a
four-finger upward transition from Card Line, multiple existing desktops as
spatial surfaces, one real card moved to another desktop, and correct membership
when entering its destination Card Line. Gesture ownership, multi-display rules
and the KDE virtual-desktop API require explicit audits before implementation.
See `KADUNCE-TABLE-1.1-CONCEPT.md`.

## Resource and budget policy

- B-Team/Sol owns bounded visual and Tette/Temperance implementation by default.
- A-Team/Astra owns cross-owner architecture, Kadunce, risky lifecycle work and
  geometry authority gates.
- Assign one coherent packet with a visible outcome, focused tests, physical
  acceptance and a stop point. Prefer focused checks over broad campaigns.
- Architecture cleanup alone does not block a usable release. A new persistent
  owner, broad compatibility matrix or scope expansion requires J's product and
  budget decision.
