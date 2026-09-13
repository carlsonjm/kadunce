# Architecture

For current completion boundaries and the reproducible fresh-build gate, read
INTEGRATION-RELEASE-GATE.md. The extraction/handoff history below records earlier
stages; its statements that runtime integration is pending are historical, not
the current status. CURRENT_STATE.md and the current-implementation section win.

## Product boundary

KWin owns real windows. Kadunce owns a logical card model, output-local
workspace sessions, input routing, and the compositor presentation derived
from them.

Card Line does not store or apply off-screen client coordinates. The Desktop
Stage uses real, reversible geometry because its windows remain simultaneously
interactive. A cross-display handoff changes physical ownership only after a
committed destination accepts the transaction.

## Golden rule

Card Line and Bento are output-local presentations, not display-type restrictions.
The tablet can enter Bento even with an external display attached. A local tablet
edge drop reserves entry during motion and invokes the same stage transition as
the shortcut on release. Card Stage releases before Bento collects restore records;
the Active source's authoritative restore record crosses that boundary explicitly,
because native frame acknowledgement may lag. Other displays' Bento sessions are
not released. Open-space tablet drops still follow Card Line behavior.

## Current implementation

September13 post-paint candidate: prePaintScreen retains transform masks and
latches animation/drop-settle continuation. postPaintScreen schedules the next
frame after KWin consumes current output damage. The latch preserves a final
endpoint frame without adding an idle loop; no motion or input policy changes.
Installed-versus-candidate status is in CURRENT_STATE.md.

September13 LIVE-PREVIEW DECISION supersedes the snapshot/readiness/startup
candidate paragraphs below, which are historical. J accepts proportional margins.
Production is again one KWin OffscreenEffect live-texture path, uniform contain
scaling, existing fan rotation/aperture and black backing. Removed retained cache,
crossfade compensation, capture-readiness gates, extra full-output damage repaint,
shader warmup and timing hooks. Native geometry/ownership and input are unchanged.
Rebuilt binary matches the previously accepted rigid-fan c063 artifact exactly.
Do not reinstate snapshot machinery to eliminate now-accepted letterboxing.

September13 readiness candidate supersedes startup preparation below (now parked).
Captured shadow margins are immutable image metadata: CrossFade's current-margin
quad gets compensated so the original frame fits the card without shadow padding.
Capture rejects missing/unready buffers and unsettled size pairs, falling back live
without delaying restoration. Visible live-card damage requests output repaint;
valid retained images stay static. No capture sweep or animation/input changes.
Focused math/readiness checks pass; J's GPU/first-visit test is pending.

September13 startup-preparation candidate: Effect schedules two resource-only
shader draws on2x2 scratch framebuffers after2500ms/250ms. A temporary passive
input spy cancels remaining work on activity; current held touch/buttons,
card motion/carry and unavailable GL also skip. No retries or GPU waits. Scratch
resources die in ShaderPreparation; no app surfaces, state or snapshot capture
participate. J's real-GPU/cold-start validation is pending. Preparation is best
effort and cannot interrupt a GPU command already submitted; input never waits
for a preparation-complete flag. Normal on-demand drawing remains authoritative.

September13 retained Active-preview candidate: explicit Active→Card Line toggle
asks the host to capture before native restore. Effect owns a bounded rendering
cache via KWin CrossFadeEffect; authoritative restore records remain controller
owned and are applied immediately as before. Captured aspect is compensated in
paint, with shader rotation after aspect compensation to avoid nonuniform-scale
shear.64MiB/eight-entry FIFO bounds retained pixel storage. Reentry refresh,
closure, release, output/membership departure and teardown discard images.
Unsupported/non-GL capture falls back live. J passed physical tests September13;
not every automatic Active-departure path captures. Card Line images are static
until refreshed on a later explicit Active exit. No ownership/input changes.

Accepted September13 interaction freeze: stack entry establishes an insertion
anchor; fresh horizontal contact movement requests one slot after existing dwell.
End seams do not become row paging. Held row paging requires physical screen-edge
contact and validates each repeat. Approach side initializes first/last insertion.
WorkspaceInputRouter owns this intent; CardWorkspaceState owns ordered insertion
plans; CardStageController owns presentation. Failed44%/pose work is archived
outside main. Placement-selection continuity is the next separate pass.

Placement-continuity candidate: prepared insertion uses DestinationCard selection
policy, retaining the existing active identity while its numeric index shifts.
Explicit model commands can still select the inserted member. The renderer and
elevation continue consuming the single model selection; there is no paint-only
selection override. Entry/exit blends open browse poses and envelope together.
The slot label reads only a revision-validated insertion and has no hit target.

Automatic native edge placement is suppressed for the lifetime of the effect,
including ordinary windows. NativeEdgePolicy saves electric-border tiling and
maximize preferences in memory, suppresses them after configuration reload, and
restores the latest preferences on unload. No persistent settings are rewritten.
Open-space window movement still remains native until intentional admission.
Explicit Shift-custom-tiling and keyboard/manual state changes are distinct paths.

Direct idle bottom gestures start in the visible bottom dock/work-area depth plus
the36px band above it. The router still passes initial contact to the client and
claims only deliberate single-finger upward motion. The held-card departure
target remains the physical bottom24px; landing clearance never narrows idle reach.

Dock safety uses one output-independent landing rule for ordinary releases and
explicit Bento bottom departures. Visible bottom dock frames supplement the
work area when a floating dock has no strut. Ordinary release within that protected
bottom zone is translated once with10px clearance after native finish; cancellation
does not apply it. Bento's departure trigger remains the actual bottom24px on
both tablet and external outputs. This changes neither ordinary dock hit testing
nor cross-output admission policy.

New eligible windows in existing Bento are admitted through a value-copy reflow
with their real restore snapshot. Admission is idempotent; failure leaves the
new window visible/native without mutating the session. An initially unready
window gets a one-shot native readiness callback, not a delay/retry timer.

Ordinary source proof reserves entry without canceling KWin's native drag.
Only an intentional edge or crossing into a card workspace acquires the carry
route. Release/extra input/cancellation retires the reservation; existing cards
still acquire immediately. Native output drift during the wait does not replace
the original restore record. After one native cancellation, generation/topology
and home output validate again; visual pickup uses the pre-cancel frame.
No per-motion native geometry enforcement is added.

Returning an ordinary-source carry to free monitor space keeps its temporary
input capture until release, but NativeDesktop placement has no card outline.
An explicit Bento departure retains its exit outline. Destination membership,
not temporary input routing, determines the released window's state.
Bottom-edge ordinary release applies one position-only work-area correction
with 10px clearance after native finish; cancellation does not schedule it.
Oversized windows retain a reachable title bar instead of resizing.

Native desktop pickup now applies on either display type. A carried face keeps
the effect and scanout blocker active even when neither layout exists; terminating
the carry clears its preview and redirection. Previously input adoption could
outlive renderer eligibility for ordinary windows.

NativeDesktop placement is explicit: same-output ordinary moves stay native even
beside Bento, while incoming cross-output windows retain existing-layout priority.
Source now uses the bottom 24 logical pixels of the actual destination
output for Bento departure, including panel area for an already-owned carry.
Ordinary panel input remains untouched. Source now lands the released window
10px above the work-area bottom; the edge trigger itself remains unchanged.
Its normal-window outline and label exist
only for a valid held reservation. Leaving the strip retires that commit callback.
Release prepares departure, publishes removal, places the native window, and
reflows survivors through existing guarded placement. Lone departure removes the
session. No new hold delay or change to dock input.

Server-decoration contact proof applies to both Wayland and Xwayland windows.
Wayland application-request proof uses the xdg-toplevel serial. Xwayland CSD
requests use a passive `_NET_WM_MOVERESIZE` observer: KWin's RootInfo filter runs
first, so a same-turn accepted native start is correlated with the exact request
window and sole revision-bound held contact. Pointer proof uses the saved press
surface and primary button; touch uses the saved touch focus/identity. No contact,
other-surface, wrong-button, keyboard and resize requests stay native. The filter
never consumes/replays requests; connection teardown removes it and its atom.
Proof enters the existing one-shot handoff, not a second movement controller.
The old extra
protocol requirement incorrectly kept Xwayland decoration drags on the legacy
path, which does not create Bento at an edge. Contact thresholds are unchanged.

Effect now owns NativeCarryRuntime alongside the existing gesture router. Native
contact admission, input lifetime, compositor projection and reserved cross-output
release are connected in the real plugin. See PRODUCTION-CARRY-INTEGRATION.md
for limited destination coverage and evidence; older probe-only descriptions are
historical. Existing Card Line routing has not yet been fully replaced.
Card Line now uses the same prepared DesktopStage receiver during motion and
release, including edge-created Bento. Its input lifetime and local stack/reorder
recognition remain in WorkspaceInputRouter/CardStageController. Stack commits
revalidate the live target and workspace revision; no-target detached release
restores the original stack. This is not completed motion unification.

Card Line local release captures final rectangles, rotations and visibility before
membership/order changes and reuses the existing 280 ms presentation transition.
CardStageController supplies the shared stack-pose calculation to painting and
capture; there is no second fan geometry formula. Full-pose transitions bypass
base-rectangle interpolation to avoid applying it twice. Visibility changes fade.
Input geometry/pickup uses the presented face; interrupted transitions can capture
their current pose. No native geometry or input lifetime is extended. Cancellation
does not settle; launcher guest rendering remains separate.
Explicit stack browsing also captures full poses before changing selection,
including an interrupted transition. Touch, mouse and wheel use the same action.
Explicit browsing cancels pending arrival expansion. Insertion steps revalidate
the live target and workspace revision, just as final insertion does; dwell timing
and gesture recognition are unchanged.

Stack slot selection now prepares an immutable CardWorkspaceState value plan from
destination handle and explicit insertion index. Preparation copies the model;
it neither changes membership nor consumes detached rollback. Each preview step
replaces the prepared plan. Release commits only that owner/revision-bound model,
while controller geometry checks still reject departed targets. Cancellation clears
the plan; repeated, foreign and stale commits reject. Card Line recognition and
dwell remain router-owned; native carry is not yet a stack-slot producer.

Monitor drop footprints reuse DesktopStage's value admission planner and the same
pixel-layout conversion as placement. Effect caches the footprint on motion and
paints a rounded outline only while its reservation validates. Paint never solves
layout or changes native geometry; minimum sizes and source geometry are now part
of reservation validation. Output/work-area clipping retains the bottom exclusion.
Tablet arrival feedback is not inferred from DesktopStage geometry.
For a native carry returning to an existing tablet Card Line, Effect copies the
released face before source cleanup. CardStageController seeds that normalized
origin only after successful admission and starts no new timer: its existing
center/expand sequence remains authoritative. Inactive/Active tablet arrivals
retain their current placement behavior. No native writes are added for motion.

Committed monitor drops can retain a short compositor-only transition from the
held rectangle to the reserved footprint. It starts only if KWin's requested
output/geometry match that footprint; client frame acknowledgement may lag.
The input route is already released. No native writes or deferred commits occur
during the 220 ms transition. A new press/touch, manual state change, topology
change, cancellation or teardown retires it. Requested placement divergence also
ends presentation. Tablet arrivals are not handled by this monitor transition.

Same-output Bento native carries reserve a visible pane by contact hit-test.
The value plan exchanges its occupant with the carried card, preserving the
existing rectangles and identity-bound restore records; both minimum sizes must
fit. Preview and commit share that plan. Returning to the original pane consumes
the drop without native placement. A swap publishes once, then uses guarded
session application and the existing carried-face settle. Bottom dock/panel space
is excluded. This is pane exchange, not stack insertion or freeform layout editing;
the displaced neighbor does not yet get its own compositor transition.

Kadunce is already a native C++ KWin effect: `Effect` derives from
`KWin::OffscreenEffect`, and `native/src/plugin.cpp` registers the native effect
factory. It is not a KWin script that still needs conversion into an effect.

The native KWin plugin currently has four runtime owners:

- `WorkspaceInputRouter`, which owns gesture recognition, pointer/touch
  transactions, dwell timers, and semantic command dispatch without access to
  compositor state;
- `CardStageController`, which owns Active, Card Line, stacks, selection,
  admission, and reversible card transactions through a typed host boundary;
- `DesktopStageController`, which owns external-display layout sessions,
  admission, rail resizing, handoff, placement observation, and restoration;
- `Effect`, which coordinates KWin lifecycle, output discovery, context export,
  shortcuts, cross-controller routing, and the remaining card renderer.

`CardLineModel`, `CardLineLayout`, and `BentoLayout` are isolated, headlessly
tested domain primitives. Tettegouche consumes Kadunce state only through the
versioned read-only context contract in `TETTEGOUCHE-CONTEXT.md`.

## Existing incremental extraction plan

The accepted behavior will be separated behind four owners while the KWin
effect becomes a thin lifecycle coordinator:

1. `CardStageController` — Active, Card Line, stacks, selection, and reversible
   card transactions. **Extracted.** It communicates only through the typed
   `CardStageHost` boundary and exposes a read-only render view.
2. `DesktopStageController` — layout sessions, minimum-size admission, rail
   resizing, cross-display acceptance, and restoration. **Extracted.** It
   communicates only through the typed `DesktopStageHost` boundary.
3. `CardRenderer` — painting, transformations, fan aperture, and HUD geometry.
4. `WorkspaceInputRouter` — gesture ownership, pointer transactions, timers,
   and semantic destinations. **Extracted.** It communicates only through the
   typed `WorkspaceInputTarget` boundary.

The coordinator owns KWin registration, output discovery, context publication,
and controller orchestration. Controllers use direct typed calls; there is no
event bus or duplicate state authority. `CardRenderer` remains the next
isolated rendering extraction in this earlier plan.

## Workspace lifetime boundary under review

Block 2's first slice introduces `CardWorkspaceSnapshot`, a value-only projection
of window UUIDs, stack membership and selection. CardStageController constructs
it; Effect's version-1 context serializer consumes it instead of reading model
indices and native-window membership directly. Snapshot values contain no KWin
pointers, render geometry, timers or action methods. Their compatibility indices
can change after closure; UUIDs remain the identity. The serializer still owns
live application metadata and presentation fields. The renderer still reads the
existing controller view, now backed by the state owner described below.

`CardWorkspaceState<Handle>` now owns the membership list and CardLineModel
together. Only it admits/removes members or replaces a session model. Its
semantic commands preserve the existing model's paging, stack, detach/cancel,
reorder and selection behavior; consumers receive const model/member views.
The production adapter uses weak KWin window handles; isolated tests use string
identities. KWin discovery/output policy, restoration and animation remain in
CardStageController. There is no second mutable membership authority.

This extraction deliberately is not a persistent workspace core. Card Stage release
explicitly clears the owner's live registry, and rebuilding resets its model. The public
`/Kadunce` endpoint is registered by `Effect` and disappears on effect unload.
Input routing is semantically separated but still uses KWin event types.

Block 3 begins with device-local cancellation: touch cleanup must not mutate
pointer transactions or their timers. Shared holds retain their initiating
device. See [input ownership](INPUT-OWNERSHIP.md) for routing, test evidence and
the remaining lifecycle/mixed-device gaps; this is not yet a unified interaction
session implementation.

CardStageHost exposes a narrow cancellation callback before release, card closure
and guest departure; Effect handles display-removal invalidation. The router
keeps consumed contact/button identities until release while discarding their
actions and timers. This drain belongs to input routing, not workspace state.
View toggle, Active entry and guest entry use the same boundary. Forwarded
pointer streams retain a set of held buttons across view changes, rather than
ending delivery ownership on the first release.
On unload, Effect explicitly cancels and destroys the input router while its
controllers are alive, before restoring windows. The isolated delivery and
full-effect virtual-tablet checks are recorded in UNLOAD-VALIDATION.md.

The snapshot and state-command boundary now exist inside the plugin, separate
from presentation progress and native window mutation. Whether identity must
survive effect unload, and therefore needs another lifetime owner, remains a
design decision. Do not add a service solely to achieve a class separation.

Compare the current renderer with KDE's QuickSceneEffect approach before
committing to a presentation rewrite. Preserve the current context and guest
protocols through adapters. See [CURRENT_STATE.md](CURRENT_STATE.md) for evidence
and provenance, [DECISIONS.md](DECISIONS.md) for decision status,
[KDE_RESEARCH.md](KDE_RESEARCH.md) for upstream evidence, and
[REFACTOR-PLAN.md](REFACTOR-PLAN.md) for the staged work.

## Handoff contract

This is the target contract, not proof of complete implementation. Managed
Bento-to-Bento transfer now prepares both Session values through
BentoSessionTransfer, requiring visible destination admission before source
removal. It publishes both without native calls between them, then applies
geometry. Rejected native drag replays the unchanged source layout. Bento-to-native
desktop now validates the output and prepares source departure through the same
value helper before publishing and guarded native placement. Card Line→monitor
now prepares existing Bento admission, commits a revision-checked source removal,
publishes destination, then releases source visuals and applies native placement.
No existing Bento means native desktop placement. Existing Bento rejection returns
the card rather than falling through to native desktop. Bento→tablet prepares
source departure and uses a one-shot receiver acceptance callback before removal.
Tablet release invalidates the placement continuation. Active tablet sessions
now publish prepared incoming membership before native placement; inactive
tablets retain native-window arrival. Edge-snap creation remains pending. Native
configure acknowledgement remains unverified; a targeted real-KWin synchronous
restore-during-placement test now passes. Logical preparation is not physical atomicity.
The shared state owner now supports prepared admission: destination identity and
revision are checked before a synchronous source-model commit, then membership
publishes without native calls. It reuses ordinary append semantics and rejects
an outstanding detached carry. Tablet transfers now use this primitive before
presentation cleanup. Transfers and ordinary launches share finishNewArrival for
guest/centered/Active completion. A held tablet card rejects incoming transfers;
post-commit interruption is consumed, not a stale-source rollback. Capturing old
preview geometry and closing an existing guest can still precede acceptance.
MonitorDropIntent now expresses existing-layout, explicit new-layout-edge and
native-desktop destinations through the same CarrySession contract. Existing
Bento takes priority; invalid existing targets reject rather than falling back.
New-layout intent carries an explicit edge and output revision, never a release
coordinate inference. This resolver and the carry contract remain headless:
production preview recognition, first-layout creation and snap/page arbitration
are not wired. Edge placement timing and companion commitment remain U4 choices.
prepareBentoAdmission is now the common value planner for empty first-layout
candidates, existing Card Line→Bento admission and Bento→Bento transfer. It checks
snapshot identity, duplicate membership and required visibility before publication.
An empty candidate includes only the arriving card, not unrelated desktop clients.
That describes current code, not the revised activation target: the user now
requires edge-snap entry to promote the destination monitor's eligible windows
together, like Ctrl+B. Prepare the complete first-layout batch before committing
the transfer; do not invoke mutating activation during admission. A lone member
uses Active presentation while retaining Bento membership. Existing Bento keeps
its normal arrival path; open-space drop without Bento stays native. Other
displays, panels and launcher guests are not recruited. See DECISIONS.md.
prepareBentoActivation now validates a caller-collected batch on a value copy:
unique valid restore records, mandatory arrival visibility when requested, and
complete visible/overflow membership with matching rectangle count. The existing
shortcut activation uses it before publishing its Session. It preserves the
existing collector, eligibility, solver, overflow and work-area geometry. Its
host preparation still precedes collection; this mutating shortcut entry point
is not an edge-transfer API. Edge arrival merge/deduplication, source transaction,
output revisions and adoption/rendering remain to be connected. Lone Active
presentation policy is not newly implemented by the planner.
transferCardWindow now accepts explicit ActivateBento intent. Without an existing
layout it collects eligible residents and merges the arriving card's authoritative
restore record exactly once through prepareBentoActivationWithArrival. It requires
arrival visibility, commits the source model, publishes the full destination, then
releases source visuals and applies guarded geometry. Rejection is not native
fallback. Existing sessions retain admission priority; default OpenSpace retains
ordinary-desktop behavior. The activation branch rejects tablet destinations and
ineligible arrivals. The current arrival record remains the established normalized
desktop restore geometry; native-titlebar snapshot adoption still needs integration.
No production input caller selects ActivateBento yet: gesture recognition and
preview validation/output revisions remain separate integration.
Card Line's existing router-owned carry now receives absolute contact positions
for pointer and touch. The controller freezes the pickup rectangle/contact and
exports a two-axis displacement. Painting follows that displacement without
scale/lift or destination attraction; only the admitted carried card gets a
cross-output Card paint route. Every pass clips to its own output. Touch and
pointer release share the existing output admission path. The headless CarrySession
is still not wired into production: native titlebar adoption, validated preview
tickets and release animation are pending. Existing dwell/reorder policy remains;
this change does not enable native-snap suppression or edge-triggered Bento.
`NativeMoveTakeover` is the tested native cancellation boundary, currently used
only by the isolated snap probe. It validates a caller-supplied source reservation,
captures observed native state, starts CarrySession, marks the synchronous finish
as takeover before canceling KWin, and rejects resurrection after interruption.
It never applies carry geometry or replaces authoritative controller restore
records. Source close/output invalidation ends the carry; outcomes are one-shot.
Effect's source reservations, actual initiating-device identification, input drain
and renderer/drop integration remain required before enabling it in production.
Native application now uses copied plans, weak handles and per-mutation
generation checks. Settling no longer issues geometry corrections. One delayed
read-only observation can restore a persistently mismatched
output session to desktop state. This is not configure acknowledgement or
cross-output transaction rollback. During restoration, topology loss can advance
through a finite deduplicated output snapshot, preserving the original restore
record. Removal notifications retire outputs before list updates; screen-added
notifications allow reuse. Client closure/manual takeover aborts without retargeting.
No surviving candidate delegates placement to KWin, without a deferred retry queue. See
NATIVE-INTERRUPTION-VALIDATION.md for the exact tested boundary.

1. Input identifies a semantic destination.
2. `CardStageController` creates a reversible transfer token.
3. `DesktopStageController` accepts or rejects it.
4. The source commits removal only after acceptance.
5. Rejection restores exact stack order, selection, and source state.

## Invariants

NativeCarryHandoff now coordinates one-turn source staging and correlated adoption
in the private probe. Successful cancellation preserves source and suppresses its
native finish; rejection invokes one fallback. Cancel retires pending work without
fallback. Effect does not consume it yet; painting/draining must be connected
before enabling takeover. See NATIVE-HANDOFF-COORDINATOR.md for exact test limits.

Native titlebar carry preparation uses `PreparedCarrySource`: an immutable,
read-only reservation over the source controller's authoritative restore state.
Active records use restore/workspace generations; Bento records use application
tokens/generations. Both validate controller identity, live membership and output
geometry. Preparation neither removes membership nor freezes the source; callers
must revalidate around native cancellation. `NativeMoveTakeover`'s observed native
snapshot is not a substitute for this pre-presentation snapshot. Effect does not
consume these reservations yet; its legacy start/finish handlers remain unchanged.
Minimized/maximized restoration fails even with only plain KWin calls and no
Kadunce controller: simultaneous maximize/minimize leaves stale client bounds.
Separating minimization until visible geometry matches passes the private control.
The candidate now uses controller-owned RestoredMinimization observers for changed
geometry. Restore issues native state/geometry once; the observer requires matching
reported and requested state/geometry before minimizing. Already-restored frames
can minimize synchronously. A 2s deadline only cancels; it never forces success.
Output retirement, manual movement, activation, newer layout and owner destruction
cancel callbacks. Effect teardown cancels old observers; newly created teardown
observers die with the controller. User explicitly accepts restored visible windows
instead of delayed disable in that interruption. No callbacks survive the owner.
This is geometry/state observation, not correlation to a Wayland configure serial.
See RESTORATION-VALIDATION.md. Candidate source only, not installed.

1. A client has one physical KWin output.
2. Card Line never mutates real client geometry.
3. Every physical mutation has one owning restore snapshot.
4. Rendering consumes state but does not mutate controllers.
5. Input routes semantic commands but does not edit models directly.
6. Other applications consume context without entering Kadunce's card registry
   or compositor ownership model.
7. Disable restores clients before unloading the effect.
