# Refactor candidate — couch validation

Updated September 11, 2026. **Stability block accepted by user.** Installed hash:
`6b752de4f4d9cedbf5ae533716c93135d6cf7c1b003ba2751df27b067a961448`.
User reports normal Tette and no obvious regression; grabbing feels smoother
(subjective, no timing change). The earlier held-touch Disable failure was fixed
by including visible AppletPopup surfaces in the panel input exemption.
User confirms successful disable while holding, then accepted the block after
the inert-release/re-enable/fresh-tap-and-hold check. Nine native tests and
source/control checks pass. Trusted repair is unchanged; no publish/promotion.

This accepts the bounded stability block, not every broader scenario below.
Unreported individual cases remain NOT RUN; monitor testing remains parked.

Save work. Use disposable windows on the tablet; monitor tests stay parked.
This is a correctness pass, not acceptance of a new swipe/animation design.

| Check | Expected result | Status |
| --- | --- | --- |
| Small window → Active → Card Line → Ctrl+Esc; repeat maximized and true fullscreen | Gutters in Active; original desktop geometry/focus restored; no black window | NOT RUN |
| Two cards, then three; page both directions; lift/cancel a stack member | Accepted 64/54 sizing and order; cancel restores stack; next gesture works | NOT RUN |
| Tette outside tap, outside drag-and-return, swipe away; reopen | Tap closes; drag is not a tap; swipe keeps Card Line; subsequent tap works | NOT RUN |
| Launch an app through Tette; change selection before delayed expansion | Correct center replacement; stale action does not activate a replacement selection | NOT RUN |
| Begin a hold, then close the affected disposable window or use Ctrl+Esc; lift finger | No delayed grab/activation; no stuck input; next click/tap works | NOT RUN |
| Mouse-held card plus secondary touch; finger-held card plus mouse click/scroll | Secondary workspace input does not commit/page the primary grab; panel/client input remains usable | NOT RUN |
| Inside Tette hold left+right, release in either order after moving outside | Both releases stay with Tette; next normal card click works | NOT RUN |

Stop on a new regression. Record the action sequence and whether failure survives
a fresh gesture; do not keep retuning timings or layer fixes over an unclear state.

## Separate safety validation gate

Read-only tray registration/startup checks and isolated unload delivery now pass.
See UNLOAD-VALIDATION.md for the full-effect virtual-tablet fixture and its limits.
Physical held-card disable/release/recovery: **USER PASS** on the hash above.
The following remains the repeatable test procedure, not a request to repeat it.

After installing the candidate and saving work, physically confirm: disable with
no held input, re-enable, then hold a disposable card with a finger and use the
mouse to disable. Lift the finger; nothing should activate from that release.
Desktop interaction and subsequent re-enable/new gestures should work normally.
Repeat with a short contact before the hold threshold. Stop on lost/stuck input,
unexpected activation or failed restoration; do not repeat a failing sequence.

User-session disable/re-enable needs explicit permission, saved work and a usable
recovery path. Do not trigger it automatically as part of this checklist.

## Sequence after this check

Unpack existing Plasma Mobile research before motion/touch redesign. Research
can proceed while physical validation is pending, but an unvalidated candidate
must not become the installed/repair baseline. Display handoff remains a separate
docked test, not something to infer from these tablet checks.
