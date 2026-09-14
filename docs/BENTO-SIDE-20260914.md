# Side/share placement candidate

Upper half of either side edge requests the larger pane; lower requests smaller.
Contact position selects intent, with18px midpoint hysteresis. Existing physical
edge/dock exclusions remain unchanged. Top-edge behavior retains the old preset.

62/38 is a curated preset, not a computed minimum-size ratio. It remains the
default for non-explicit layout activation. Side placement aims at thirds and
clamps against native minimum sizes plus the existing14px layout gap. Preview
shows the exact adjusted plan. Infeasible placement gets no accepting preview.

The dragged card receives a full-height side pane. Remaining visible companions
use existing Bento presets inside the remainder, up to eight total cards. Both
preset orientations and existing two-pane alternatives are considered. The solver
preserves companion order and never invents overflow to force an explicit snap.
It does not search every possible assignment of apps to slots; infeasible ordered
patterns are rejected. Physical large-monitor acceptance remains outstanding.

J reports a new Spotify launch stayed native with Bento open. The side planner
could reject expanded membership without trying ordinary required-arrival
admission. Launch-only correction clears the remembered split on the candidate
copy and retries the established planner, allowing its normal overflow policy.
Explicit edge reservations remain strict. If even normal admission cannot fit
the new app, it remains visible/native rather than disappearing. Actual Spotify
minimum-size failure was not captured; the regression path is covered with a
constrained test client. Candidate installer: ../install-kadunce-bento-launch-20260914.sh.

One visible card fills the workspace but retains side/share intent. Later arrival
or restoration reuses it. An explicit interior pane swap clears that remembered
side preference, preserving the swap on later ordinary reflow. No native KDE edge
placement is reenabled; touch divider is the next separate feature.

## Evidence

- Native production build; Bento layout/transfer tests and verify-control pass.
- Pure layout checks cover four edge choices, midpoint hysteresis, minimum sizes,
  impossible pairs, single-card fill, and two through eight cards at3840x2160.
- /tmp/kadunce-unload-test.AlJfji/session.log: ordinary first entry, repeated
  pointer/touch side snaps, preview=committed geometry, survivor/restore preference.
- /tmp/kadunce-unload-test.Y1taab/session.log: same with virtual tablet Active
  entry plus a Card Line held touch drop. Virtual-0 predicate restored afterward.
- No physical acceptance yet. Do not infer monitor, gaming or divider acceptance.
- Final production regression: /tmp/kadunce-unload-test.kIWI5O/session.log passes
  existing pointer/touch pane exchange, preview withdrawal, dock exclusion and
  original restoration. Installer --check passes hashes and live safety control.

Installer: ../install-kadunce-bento-side-20260914.sh. Exact hashes are pinned in
the installer and local work/kadunce-bento-side-20260914 bundle. Rollback is J's
accepted membership d2acb77c; main remains ead1fe3 until explicitly frozen/pushed.
# Edge overflow correction

J confirmed closing Spotify restores ordinary snap. The all-residents-fit rule
is superseded: choose the largest fitting subset that keeps the edge card visible,
and minimize non-fitting residents using existing overflow/restoration. Singleton
fills Active space. Required newly launched apps cannot silently become overflow.

Candidate SHA256: cd7df839bcc8fe071fb54876de37c14a873d12a9b603cba7ab9205a50d9a8fb1.
Installed rollback: 23baa068dde88a2a6c1266b788f84a2561776a92d6fabaab7bbe839e3a6f65c0.
Installer: ../install-kadunce-bento-overflow-20260914.sh; --rollback restores that binary.
Not installed or pushed. Pure layout/transfer and control checks pass. Private
/tmp/kadunce-unload-test.zARsqi/session.log passes ordinary entry, repeat side/share,
singleton recovery, constrained launch, and mouse/touch edge overflow plus release.
Live safety/hash --check passed. J's Spotify/tablet acceptance remains pending.
Earlier Zzq8R9 fixture expected a singleton unnecessarily; a 1250px minimum fixture
now deliberately requires overflow on the 1280px private display.
