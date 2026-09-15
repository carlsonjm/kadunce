# Current state

## September 15 — Itasca visual and Ambient freeze

J installed the whole-suite candidate, rebooted, and physically passed the final
state. The accepted production heads at the start of the EOD documentation pass
are:

- Kadunce **192d8b4** — Ghost White tray foreground, lowercase spatial hints,
  and the completed visual/motion audit on top of the Table 1.1 product record.
- Tettegouche **7f54caa** — responsive Ambient media/transfers, real providers,
  Lucide action chrome, completion feedback, and corrected packaged icon paths.
- Temperance **10fb70f** — Lucide action chrome, notification actions and
  individual dismissal, corrected notification/tray copy, packaged icon fixes,
  and restored event-driven left-boundary measurement.

All three preserve the accepted center-dock geometry. Tette owns ongoing context
on the right: **what matters now**. Temperance owns transient notification/events
on the left: **what changed**. Source services remain authoritative for state and
actions. Width reveals information in the order controls, title, artist, runtime;
available width is measured rather than capped to a fixed descriptive tier.

The shared visual grammar is `ITASCA-VISUAL-LANGUAGE.md`: Lucide 1.46.0 for
suite-owned action chrome, Ghost White `#F8F8FF`, pills for controls, rounded
boxes for information, lowercase labels, responsive fit, and bounded motion.
Custom identity work remains protected: Tette Dot, Temperance Bell, Weather,
tray, Speaker, performance selector, and Kadunce stacked-card tray mark. Provider
icons and proper names remain external identity and are not normalized.

Kadunce's accepted spatial behavior is unchanged by its visual pass. Its custom
card, fan, Bento, rail and preview geometry remains the interaction language.
The persistent tray enable/disable control remains release-critical and wired to
`graphical-session.target`. A separate motion-policy gap remains: custom
compositor durations do not yet follow platform animation scaling or reduced
motion. See `VISUAL-AUDIT-20260915.md`.

## Validation and acceptance

- Tettegouche exact-head build, source checks, 12/12 CTests and diff checks pass.
  The protected Dot applet source is unchanged.
- Temperance exact-head build, 5/5 CTests and diff checks pass. Its focused panel
  boundary test passes; protected custom glyph sources remain unchanged.
- Kadunce production build, five focused motion/layout/paint CTests, source
  checks, mandatory control verification, staged dependency check and diff checks
  pass. J's whole-suite reboot and physical pass supplies the final visual
  acceptance for this batch.

No accepted production change from this batch remains only in a release
worktree. Older experiments and unrelated local Files branches are not part of
the freeze.

## Next bounded work

1. **Tette component polish:** align Files/drawer control affordances, fix wasted
   width and premature truncation, replace remaining raw action glyphs, and tune
   media spacing without changing accepted responsive capability order.
2. **Temperance closure:** capture settled popup/dock geometry before changing the
   reported gap; then add a bounded non-notification event-source slice. Do not
   fake the gap with internal content padding.
3. **Tette Files completion:** external drag/drop, Open With and properties;
   thumbnails/recursive search/Recent; then removable and optional network
   storage lifecycle as separate packets.
4. **Release integration:** clean builds and package provenance for all repos,
   install/uninstall or rollback, persistent Kadunce safety control, tablet-only
   and docked acceptance, scaling/rotation, focus and Meta cleanup.
5. **Debug block:** reproduce any remaining report on current main after major
   work. Do not reopen cleared R1 reports without a current failure.

The Table 1.1 concept remains recorded in `KADUNCE-TABLE-1.1-CONCEPT.md` as
post-launch direction. No Table implementation packet is active before 1.0.

Full daily handoff and continuation order: `EOD-20260915.md`.

## Safety

For Kadunce changes, run `bash tests/verify-control.sh`; after installation in the
graphical session, run `bash tests/verify-live-control.sh`. Do not infer a missing
kill switch from a sandbox or D-Bus transport failure. Do not log out, stop the
graphical session, or toggle the effect as a test without J's authorization.
