# A2 — tablet Bento and Card Line ownership

Accepted source candidate **5e91671** from A1 **0d970c9**, replayed as **d2a0357**
on PM main **bfe761b**. J physically passed constrained launches, stack retention /
large-pane selection, monitor isolation and lifecycle on September 16. Zen may
use the small pane when its minimum permits it. Production replay is identical;
see `CHECKPOINT-20260916.md` for publication checks and remaining presentation defect.

## Failure and contract

A failed required-arrival Bento solve returned `true` without adding the newcomer
to the session. Effect stopped routing at that result, leaving a constrained tablet
launch unmanaged. Separately, Card Line toggle restored and deleted tablet Bento,
then rediscovered standalone windows; grouping and original ownership were lost.

- Tablet launch still tries the existing minimum-size-aware pane planner first.
  If no pane fits, retain the newcomer as overflow, request the existing Active
  target, and minimize after KWin reports that accepted geometry. There is no
  alternate pane geometry or attempt to force an application-specific minimum.
- Tablet Bento projects to one Card Line stack using its existing membership and
  native restore records. The largest pane supplies the initial selected face;
  remaining visible panes and retained records have deterministic order. Source
  removal and destination membership commit before native visibility changes.
- Card Line's existing state/model owns the stack. No persistent workspace owner,
  daemon, background session store or second card registry is introduced.
- Release retains original geometry and minimized state. Pending preparation
  observers are retired when ownership leaves Bento, so a delayed minimize cannot
  outlive release. Active visits reuse the same retained records.
- Ordinary cross-output adoption into existing tablet Bento now records KWin's
  committed tablet placement before pane sizing, matching A1's Card Line rule.
  Rejected adoption leaves the monitor source intact. Monitor admission, monitor
  Bento layout and monitor presentation policy remain unchanged.

`ownsWindow()` reports retained membership including overflow; `managesWindow()`
continues to mean a visible native pane for existing input/render consumers.
The distinction matters when checking a prepared minimized card.

## Scope and validation

Production changes are confined to CardWorkspaceState, CardStageController,
DesktopStageController and the Effect toggle adapter. Existing layout solvers,
renderer, motion durations, input thresholds, dock-clearance geometry and tray
control are preserved. Tettegouche and Temperance are not changed.

Focused model coverage exercises transactional stack import, rejection, duplicate
identities, deterministic selection/order, closure and detach/cancel. The private
runtime fixture uses actual production controllers and native Qt clients on two
virtual outputs. It exercises constrained/oversized launch, round-trip grouping,
Active visits, original restore records, monitor isolation, cross-output rejection
and adoption, release, and pending-preparation retirement. The final A2 private
runtime passed (exit 0): `/tmp/kadunce-unload-test.5JeSZr/session-retry.log`.
The initial socket denial was an environment restriction; the approved run used
only the isolated bus/config/data/runtime. Earlier fixture assertions were corrected
to distinguish retained ownership from visible panes and to inspect KWin's actual
accepted Active size; diagnostic logs are retained in that evidence directory.
Existing A1 initial-entry and cross-output ownership runtime also passed against
A2: `/tmp/kadunce-unload-test.5JeSZr/a1-regression.log`.
Production build and eight focused CTests passed; source guards, shell syntax,
diff checks and mandatory control checks passed. The persistent control package
still uses `graphical-session.target`. Physical post-install live safety remains
J's check, not a claim from the private runtime.

KWin's accepted Active target can be smaller than a client's advertised minimum.
The test checks the native accepted/configured target rather than assuming KWin
will clamp it. This preserves the existing Active sizing policy. No claim is made
about physical gesture recognition, Plasma dock painting or Zen-specific behavior
beyond J's September 16 physical ownership pass. Bento-derived Card Line presentation
remains defective and is not covered by that acceptance.

## Rollback and physical review

Rollback is published A1 **0d970c9**, reinstalled through its normal installer and
session procedure. No reset, history rewrite or branch deletion is needed.

1. Verify the persistent tray control; run `tests/verify-live-control.sh` in the
   graphical session after installation.
2. Tablet-only: Active window, Ghostty in Bento, launch Zen. It must receive a
   fitting pane or become an owned prepared card; no unmanaged window above Bento.
3. Enter Card Line from tablet Bento. All Bento members form one stack with the
   large-pane member selected. Browse members, visit Active, return to Card Line,
   then return to Bento. Verify membership, ordinary release and original states.
4. Keep a monitor Bento session open during tablet transitions. Its layout and
   behavior must remain unchanged. Monitors must not expose Card Line.
5. Adopt an ordinary monitor window into tablet Bento, then enter Card Line and
   release: ordinary geometry remains on tablet. Cancellation before commitment
   stays on monitor. Verify normal dock clearance and the persistent control.
6. Release/disable immediately after a constrained launch. No late minimize,
   stranded member or stale presentation may survive ownership release.

Ownership is physically accepted. Center/transfer geometry, stack hit-zones,
labels and the Bento-derived live presentation defect remain separate open work.
No unfinished A2.1 production change is included.
