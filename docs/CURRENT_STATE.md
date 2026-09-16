# Current state

## September 16 — A1 published; A2 physical-review candidate

J passed A1 tests 1–3: persistent control, committed cross-display release to the
tablet, and immediate ownership of every Card Line member. A1 **0565ccf** was
replayed unchanged as **02e6a6f** on PM's **8d3a0b9**, then published with acceptance
docs at **0d970c9** by normal fast-forward. PM's roadmap/checklist are preserved.
Fresh production build, eight focused tests and source/control checks passed.

A2 is isolated on `a/ownership-a2` from **0d970c9**. Tablet Bento now transfers its
retained members into one Card Line stack with the largest pane selected. A new
constrained tablet window uses the existing pane planner or remains owned as a
prepared Active-sized minimized card. Ordinary cross-display adoption into existing
tablet Bento uses the same committed tablet-origin rule as A1. Release cancels
pending preparation and preserves ordinary geometry/minimized state. Existing
session and stack owners suffice; no new persistence owner is introduced.

See `OWNERSHIP-A2-20260916.md` for exact evidence and physical checklist. The private
two-output runtime passes launch, grouping/Active round trips, origin preservation,
monitor isolation, rejected/committed cross-display adoption and immediate release.
Physical Zen/Ghostty behavior, gestures and Plasma dock painting remain J's gate.
A2 is not installed or pushed; installed acceptance remains A1. Rollback is
published **0d970c9**. No Tettegouche/Temperance, geometry library, renderer, motion,
input timing or safety-control changes are included in A2.

After A2 handoff, the separately authorized center-slot architecture review may
begin. Do not mix its implementation into this ownership candidate.

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

The rolling execution plan is `NEXT-ROADMAP.md`; update its section status and
planning number after every accepted block. Current forecast is **6 focused
working days**, with a 5–7 day expected range.

1. **Debug and polish:** A1 and A2 ownership behavior passed J's physical tests.
   Zen correctly used the small Bento pane because its minimum size did not
   require the large pane. The remaining Kadunce follow-up is Bento-derived Card
   Line presentation geometry: ownership/stack order survive, but the live card
   content is undersized inside large black areas. Tette B1 remains unaccepted
   after centered-dock and transfer-completion failures.
2. **Missing features:** Files B properties/discovery, Files C storage lifecycle,
   Temperance event sources, and Steam/external libraries if retained for 1.0.
3. **Refactor audit and consumer installer.**
4. **Mac mini website hosting with standard secure tablet control.**
5. **Interactive website and release:** full-screen Itasca desktop, guided entry,
   active Kadunce/Tettegouche/Temperance cards and representative interactions.

The former Files A slice is deleted; external drag-and-drop and Open With are not
launch-roadmap items. Properties is part of Files B. The Mac mini is not a custom
controller/build service. J's personal-site migration is later and nonblocking.

Live post-freeze checklist: `POST-FREEZE-TEST-20260915.md`.

## Safety

For Kadunce changes, run `bash tests/verify-control.sh`; after installation in the
graphical session, run `bash tests/verify-live-control.sh`. Do not infer a missing
kill switch from a sandbox or D-Bus transport failure. Do not log out, stop the
graphical session, or toggle the effect as a test without J's authorization.
