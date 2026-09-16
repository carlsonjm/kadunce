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

The rolling execution plan is `NEXT-ROADMAP.md`; update its section status and
planning number after every accepted block. Current forecast is **6 focused
working days**, with a 5–7 day expected range.

1. **Debug and polish:** A1 retained-ownership fixes passed J's physical tests.
   The active gate is tablet Card Line/Bento ownership: new-window routing and
   retaining Bento cards as a Card Line stack with the large pane on top. Tette B1
   remains unaccepted after centered-dock and transfer-completion failures.
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
