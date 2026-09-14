# Next roadmap — stopping point, September 12, 2026

Latest accepted main checkpoint: FREEZE-20260912-EDGE-STABILIZATION.md.
J passed the installed38a2fbb5 candidate and authorized freeze/push. The old
installed hashes and unresolved-status sections below are historical evidence;
CURRENT_STATE.md and MVP-RELEASE-SCOPE.md define present status and assignments.

Execution scope is now **MVP-RELEASE-SCOPE.md**: dock safety + new-app admission,
J acceptance, then freeze/push. ENGINEERING-BLOCKS.md retains reference packets.
This file retains detailed backlog/design evidence, including E12-E15.
Historical “next” directives below do not override that queue or CURRENT_STATE.

Primary restart document after CURRENT_STATE.md. This supersedes older unchecked
Tomorrow lists as execution order, not as permission to install/publish.
HEAVY-REMAINING.md is the concise architecture-heavy work split; current
installed/source evidence always comes from CURRENT_STATE.md.
Latest physical checkpoint: f30825c0 return correction passed all three user
tests (tablet Active→monitor, monitor transit round-trip, edge/exit basics).
Deferred new failure: after bottom exit, the returned ordinary window could not
re-enter existing Bento; KDE snapping acted over that layout. Reproduce the full
exit→fresh drag→edge sequence, not only fresh-window entry. No cause confirmed
for that physical report. K1 experiments and a separate reproducible native-touch
lifecycle failure are recorded in K1-TAKEOVER-AUDIT.md and CURRENT_STATE.md;
unproven production changes were withdrawn. See also NATIVE-RETURN-TEST.md.

Latest checkpoint: user installed and physically passed Xwayland candidate
7248d3fb, including landing clearance. Ordinary native-until-entry ownership is
implemented and privately validated; candidate 9b84cd5a is packaged for opt-in
physical testing with rollback to 7248d3fb. See NATIVE-ENTRY-TEST.md and CURRENT_STATE.
Occupied-tablet contention and new-app admission remain next.
The raw cleanup release also occurs without Kadunce: native baseline and actual
button-action tests distinguish it from accidental activation. User approved
native-equivalent behavior; bounded matched cleanup plus no-action/fresh-click
checks now gate this path. See XWAYLAND-TEST.md. No live installation or push.
Older checkpoint details below are historical; CURRENT_STATE has current hashes.

Resumed September 12: live blurred-state inspection showed no Kadunce renderer,
layout or carry, and no Overview. The video wallpaper's own BlurMode=1 explains
blur with an active window; wallpaper settings unchanged. P0 native ownership
and GPT edge entry remain unresolved. A bounded nativeMoveTrace has been added
to source to distinguish native fallback, proven pickup, adoption and drop.
P2 installed and physically passed bottom exit/pullback. Source now adds 10px
landing clearance above work-area bottom, independently of the edge trigger.
GPT trace captured: X11Window, decorated=false, native-fallback without proof or
adoption (both ordinary and card starts). Next P0 needs app-drawn Xwayland move
correlation, not more layout changes. See NATIVE-MOVE-TRACE-20260912.jsonl.
Also open: newly launched Ghostty failed to join monitor single-card Bento;
include new-launch admission in the production gate, separate from drag admission.

## Tonight's honest checkpoint

Installed SHA256: b2b65253e02d3e33ba73334d27b2f7c7776800642bfd13db6d0a106986dacd73.
Local main remains a74991f with the intentional uncommitted overhaul. No new push
or release tag. Source and opt-in bundles are saved, but this is NOT release-ready.

Physical wins: foundational touch pickup; card transfers; Bento resize/reorder;
single remaining member becoming Active; Ctrl+B on either focused display;
tablet-only Ctrl+S. Latest update removed drag lag. Preserve these.

Latest physical failures override automated success:

- Ordinary/released windows still get a grey placement outline during movement;
  desktop remains blurred. Layout membership alone is insufficient evidence of
  native behavior. Earlier live snapshot showed both stages inactive.
- GPT on monitor failed edge entry. User suspects this and the ordinary-window
  behavior share an ownership problem. Investigate together; cause unproven.
- With tablet Active occupied, an incoming window edge attempt glitched, fought
  placement, and returned to monitor. User then snapped the existing Active card.
- Bottom departure works only in a narrow strip above the dock. User rejects that
  target: wants the actual bottom screen edge, including carrying over the dock.

14 CTests/nine private runtime routes/source/control gates passed for this build
at /tmp/kadunce-integrated-carry.Sitj0g. They do not prove app-specific protocol,
physical input, blur, frame pacing, or the missing occupied-tablet scenario.

## Order of work — production correctness first

### P0. Reproduce and trace native/card ownership (next bounded task)

Files: NativeMoveObserver.h, NativeCarryRuntime.h, NativeCarryHandoff.h,
Effect.cpp/.h, DesktopStageController.cpp/.h; trace rendering eligibility,
redirection/elevation, destination reservation and restoration separately.

Capture bounded transition logs for GPT, Ghostty and Discord: display, native
protocol/decoration, initiating mouse/touch, membership before pickup, adoption,
edge preview, commit/reject, release, renderer/blur state. No per-frame log flood.
Compare untouched ordinary window, released Bento window and existing card.
Determine who owns the blur; do not assume KDE or Kadunce from a screenshot.

Target contract: ordinary movement stays native until intentional entry is
accepted. No card outline in open space; no forced card presentation simply
because Kadunce is enabled. Explicit edge entry must still be available after
Ctrl+Esc. Do not solve this with an app-name blacklist, permanent exclusion after
release, another delay, or disabling native snapping globally for ordinary windows.

Acceptance: GPT/Discord/Ghostty normal drag + edge entry, before/after release,
mouse/touch where supported, both displays. No stale outline/blur at rest. Scope
first implementation to this ownership boundary, not new motion tuning.

### P1. One transaction for occupied-tablet Bento entry

Build on P0. A monitor window approaching tablet edge while Active exists must
reserve the incoming window AND tablet residents before releasing either owner.
Preserve each original restore snapshot. Reject safely before source departure;
never bounce between source/target geometry writers or resurrect old placement.
Open-space arrival and deliberate edge entry are different intentions.

Acceptance: tablet inactive/Active/Card Line/Bento × incoming ordinary/card/Bento
window. Edge commits exactly once; rejection, target closure, minimum sizes,
unplug and disable preserve identity and restoration. No monitor bounce/glitch.

### P2. Actual bottom-edge exit (revised user-approved target)

Replace the 24px above-dock strip with the true bottom display edge for an
already-held monitor Bento card. Carry may cross the dock. Ordinary dock taps,
launches and bottom gestures without a held card must remain unchanged. Test the
Z13 direct-edge and Plasma-native backends; do not remove dock protection globally.
Use one preview/commit target; leaving it cancels. Retain Return to desktop label
and normal-window outline only for explicit exit, not ordinary movement.

Acceptance: lone card exits layout; multi-card departure reflows survivors; the
detached window stays ordinary; side/top re-entry works; pullback cancels; no
accidental dock activation from the held drag's release. Cover actual bottom
corners, scaled outputs and dock auto-hide. Any exact edge-band width is an
implementation/test choice, not a new hold requirement.

### P3. Production regression gate and real freeze

Run fresh-source tests plus new P0–P2 scenarios. User verifies real desktop blur,
GPT titlebar entry, occupied tablet, actual bottom exit and rapid repeated actions.
Repeat fullscreen/maximize/minimize/manual resize, monitor handoff/unplug, guest
launch/cancel, held-card disable and recovery. Check independent kill switch and
graphical-session startup wiring. Preserve hard tablet cutoff for passive cards.

Only after a physical pass: separate source commit, installed acceptance and
trusted-repair promotion decisions. Ask before installation/restart or push; do
not promote this partial-pass binary to repair. Publish only intended files and
verify the remote revision after an authorized push.

## After correctness — finish the unified-card experience

### U1. Ordered stacks and missing native-to-stack admission

Existing Card Line insertion is implemented, not a blank slate. Complete native
carry destination integration with the same identity/revision-bound insertion
plan. Fix space-fill timing and left-neighbor/right-default bias. N members need
N+1 intentional slots from either side. Test detach/reinsert, last member,
closed target, reverse approach and cancellation without losing order/focus.
Carry-time browsing to offscreen stacks remains a design checkpoint; do not
invent a mandatory rail or companion chooser without agreement.

### U2. Continuous motion and touch polish

Use KDE_RESEARCH.md, KDE-REUSE-AUDIT.md and UNIFIED-CARD-BLOCKS.md before choosing
plumbing. Preserve free two-axis carry; magnetism guides geometry, never takes
control from the finger. Address displaced-neighbor motion, tablet arrival
feedback, pair/triple/Tette transitions, fast flicks, reversals and regrabs using
measured input/presented pose/frame intervals. No wholesale timing rewrite.
The rejected quick-swipe build must not return. Physical tests, not universal
claims about iPad timing, decide acceptance.

### U3. Deferred structural choices

Persistent stacks across full release/unload are NOT implemented; decide lifetime
and reconciliation before building persistence. Additional renderer extraction is
optional architecture work, not a prerequisite to every bug fix. Bento and Card
Line remain arrangements of cards, not separate competing membership systems.
Multi-monitor/hotplug/rotation coverage remains part of acceptance, not assumed
complete from two virtual outputs. Do not alter Z13 touch policy from Kadunce.

## Ecosystem queue — after P3, with independent work packets

Detailed prior source findings: ../../ECOSYSTEM-NEXT-UPDATE.md. Recheck source
before implementation; these findings are from September 11, not current tests.

| Queue | Owner / next action | Acceptance |
| --- | --- | --- |
| E1 Meta open AND close | Tette: one semantic toggle across widget, shortcut and duplicate instance | Rapid repeats, guest cleanup, launch in flight, bridge absent |
| E2 Fullscreen dock access | Plasma/Temperance + Tette/Kadunce coordination | Meta exposes reachable dock/search without minimizing/retiling game; dismissal restores focus; monitor selection/input grabs |
| E3 Notifications disabled → stock presenter | Temperance: explicit applet-presenter handback; investigated implementation in `../../temperance/docs/NOTIFICATION-PRESENTER-HANDOFF.md` | Retain Plasma's shared notification server and gate the visual-owner switch on stock-applet readiness. No competing daemons, lost/duplicate alerts; both toggle directions, startup, DND/history/actions |
| E4 Tette Active sizing | Tette UI + versioned guest contract | Same activation gesture; query/focus/scroll retained. Drawer auto-expand/conditional shrink remains a design decision |
| E5 Desktop/Bento Tette | Tette standalone fallback + optional desktop guest design | Mouse-only use, correct display/launch destination, preserve Bento, no forced tablet Card Line |
| E6 Slow launches | Tette launch states + Kadunce readiness | Affinity splash→main, Discord startup, Spotify, cancellation/timeouts. Correlate identity/readiness; no blind longer timeout |
| E7 Steam/external games | Tette catalogue/launch; Steam owns game/Proton/library resolution | Cold/warm Steam, helper vs game window, unavailable external drive, mount/recovery, no empty fallback dirs or boot dependencies |
| E8 Truthful power status | Temperance read-only status; separate Z13 hardware audit | AC charging/holding/discharging/unknown; no claim that bolt proves electrical bypass. Battery health cause unverified |
| E9 Banner sizing | Temperance scoped visual audit | Short/long/image/action/critical/grouped alerts at both scales; preserve touch targets |
| E10 Search/child presentation polish | Tette, retain passed ranking/scope and independent child actions | Keyboard/touch paging, no parent highlight swallowing children, correct settings destination, apps-first/search fallback preserved |
| E11 Packaging/maintenance | Three independent repos/packages; distribution bundle decision later | README/artwork/descriptions match package, reproducible install/uninstall, compatibility checks and independent recovery |
| E13 Temperance surfaces above Active cards | Temperance popup classification + Kadunce stacking integration; reproduce before choosing the owner | With Kadunce enabled and an Active card visible, Temperance system surfaces open visibly above the card and accept input. Closing restores the prior card focus. Cover notifications, Control Center, organized tray and weather; do not make every ordinary window globally keep-above |
| E14 Bluetooth Add New Device | Temperance Bluetooth section; launch KDE's native pairing workflow | A clear Add new device action opens the intended Bluetooth discovery/pairing surface on the correct display. Existing connected-device rows and direct settings access remain intact; Temperance does not duplicate the pairing stack |
| E15 Bell badge micro-alignment | Temperance compact panel delegate | Preserve the centered bell; keep exactly 1 px between bell and unread counter, and increase the counter font by 1 pt. Verify 1-, 2- and 3-digit counts at supported panel scales without overlap, clipping or rail-height growth |

Maintenance follow-ups: system-corner add-on compatibility/repair and separate
repo ownership remain to verify against current state; do not redo already-passed
rounding repairs. Package-update-triggered recovery is a later integration goal,
not permission to install a global hook now. Previously passed tray icons, power
pills, tooltip fixes, weather/bell alignment and touch tap/swipe routing are
regression contracts, not pending redesigns. Local description edits/publication
status must be checked before claiming they were pushed.

Optional/proposal only: local recent-app suggestions with retention/clear/opt-out,
companion picker, persistent workspace service. Keep Just type uncluttered.
External-drive recovery checklist and any hardware health audit are separate
machine-maintenance tasks, not prerequisites to shipping search UI.

## Handoff / effort discipline

Start with CURRENT_STATE → this roadmap → relevant architecture/contract and code.
Next agent gets P0 only until evidence determines the smallest fix. Heavy work
owns input, state, rendering, restoration and guest protocol changes. Lighter
work can take explicitly named docs, test-matrix, metadata or banner-layout
packets with exact files and stop conditions. Do not spawn/delegate or change
models merely because this document describes work lanes.

Every packet ends with source vs installed hash, tests vs physical acceptance,
remaining uncertainty and next bounded task. No endless “next” without a gate.

## E12. Tettegouche All Files / file interaction — production roadmap, September 12

Consolidated from CashyOS, Pillow Talk and the September 12 user direction.
Pending design/production work, not implemented parity or a release claim.
This extends E4/E10 after the existing P3 correctness gate; it does not reorder
P0–P3. E4's drawer expansion question is now resolved for entry: both Browse
Everything and All Files expand to Active sizing. Collapse ownership and job
lifetime still require the bounded decisions below.

### Product intent and visual contract

Make file interaction part of Tette's intelligence and Active workspace. Just
Type finds an object and offers its actions; All Files is the explicit visual
fallback for exploring and managing the real filesystem. Browse Everything
continues to browse applications. Keep these purposes recognizable and connected.

Preserve the minimal black Tette card, existing Card Line form, outline, spacing
and motion language. Both browse modes expand to Active sizing inside that same
visual language. No extra permanent chrome or pill rows. Contextual text actions
at the top edge or touch-friendly badge/pill actions beside a selected file are
valid treatments to test, appearing only when useful and avoiding window controls.
The latest restraint direction supersedes earlier exploratory toolbar mockups.

Current source calls an indexed-search scope “All files.” The new browser must
have a distinct mode/entry contract without silently removing that scope or
changing accepted ranking. Browsing must reach accessible unindexed directories;
search-provider coverage is not proof of filesystem coverage. Review naming and
migration with the existing SEARCH-CONTRACT before implementing either entry.

### UX states and interaction contract

| State | Presentation and behavior | Exit / acceptance |
| --- | --- | --- |
| Opened / ready | Centered Just Type, immediately ready; quiet browse entries | Typing moves search to the top and reveals results using existing motion |
| Just Type results | Sparse ranked destinations; preserve scope, identity pinning and independent child actions | Tapping a file selects that exact object and reveals contextual actions; no automatic browser switch or file launch |
| File selected | Temporary popup/anchored actions, optional preview and useful file identity/path | Explicit Open, Open With, Show in folder, Copy, Move, Rename, Trash and More as applicable; dismiss restores query, scroll and focus |
| All Files | Active-sized card; Places/devices sidebar, location header and touch-sized file canvas | Folder navigation, Back/Forward/Up, tappable breadcrumbs and editable path/URL; return to prior search without losing context |
| Selection / operation | Single or explicit multi-select; contextual selection count/actions; progress only while relevant | Action set reflects the whole selection; conflicts, errors and cancellation remain attached to the actual job |
| Empty / unavailable | Distinguish empty directory, no search matches, loading, denied access, missing item, offline location and disconnected device | Offer relevant retry/back/mount/authentication; never present an error as an empty folder or silently redirect an action |

Just Type actions vary by MIME type, installed handlers, permissions, protocol
and current availability. Examples: image Open/Edit/Share, archive Open/Extract,
folder Open/Move/Rename. Edit, Share and Extract appear only when a supported
handler exists; generic operations remain reachable through More. Use existing
KDE associations rather than hardcoded extensions or launching arbitrary paths.
Keyboard activation offers equivalent explicit actions without slowing existing
application-result launch. Test the final key mapping and focus order.

All Files uses Home, common user folders, Trash, available Places, removable
storage and network locations from the backend. Sidebar belongs to browsing,
not ordinary search; at narrow widths it may be revealed contextually. Preserve
location context while scrolling. Provide touch selection and a visible actions
entry without requiring hover, Ctrl/Shift or right-click. Mouse and keyboard
navigation, shortcuts and native clipboard/drag interoperability remain supported.

### Backend reuse and ownership

Use the KDE infrastructure underlying Dolphin behind a narrow C++/QML adapter;
do not embed Dolphin's window or assume Dolphin exposes a ready-made headless
API. Proposed reusable pieces, subject to installed-version feasibility:

- Directory listing/metadata: KDirModel with KDirLister and KFileItem; expose
  stable URL-based destinations and reconcile rename/removal, never act on a
  stale row number. [KDirModel](https://api.kde.org/kdirmodel.html).
- Places/devices: KFilePlacesModel for places and device setup/teardown state;
  verify the required Solid/authentication integration and accessible QML roles.
  [KFilePlacesModel](https://api.kde.org/kfileplacesmodel.html).
- MIME-aware launch/actions: adapt KFileItemActions and the existing KIO launch
  path; test QWidget/QMenu dependencies, parenting and the installed API version
  before choosing a touch presentation. Full service-menu UI remains deferred.
  [KFileItemActions](https://api.kde.org/kfileitemactions.html).
- Thumbnails: evaluate KIO::PreviewJob for the QML surface, bounded requests,
  cancellation and icon fallbacks; inspect KFilePreviewGenerator only where its
  view integration fits. [PreviewJob](https://api.kde.org/kio-previewjob.html).
- Mutations: use KIO jobs for copy/move/rename/trash/delete and existing KDE
  clipboard/drop, conflict and authentication mechanisms. Evaluate recording
  supported operations through FileUndoManager; do not promise undo for permanent
  deletion or every protocol. [CopyJob](https://api.kde.org/kio-copyjob.html),
  [FileUndoManager](https://api.kde.org/kio-fileundomanager.html).

Tette owns browsing, selection and action presentation. KDE owns filesystem and
protocol semantics; Kadunce owns workspace geometry, focus and guest admission.
Temperance integration is limited to established notification/progress ownership;
no competing notification presenter. Reuse current search providers/indexing,
with explicitly scoped current-location search and truthful coverage limits.
Do not build a new crawler, filesystem engine, permission model or mount daemon.

### Must-have Dolphin parity for V1

- Create files/folders; rename, copy, cut/paste, move, duplicate and delete.
  Trash is the normal destructive action; permanent delete is explicit with a
  clear confirmation. Restore from Trash and expose supported undo accurately.
- Touch and keyboard multi-select; drag and drop within the browser and with
  other applications; explicit destination and copy/move intent, cancellation,
  collision handling and partial-failure reporting through backend semantics.
- Mount/unmount/eject supported removable/external devices, including busy,
  disconnected and authentication-required states; no automatic boot mounts.
- File thumbnails/previews with fallbacks; Open With and MIME-aware actions;
  properties and permissions with truthful read-only/unsupported states.
- Hidden-files toggle, sorting, grouping, filtering and search within the current
  location with visible scope; breadcrumb/editable-path navigation and history.
- Basic network/KIO destinations using installed supported workers; validate
  authentication, offline recovery and operations per protocol. Do not claim
  universal protocol parity from a successful local-files demo.
- Visible job progress/cancel, actionable errors and conflict choices; no silent
  overwrite, false completion or success notification after a failed operation.

### Deferred power-user features and anti-goals

Defer tabs, split view, folder-tree and rich information panels (tags, ratings,
comments), integrated terminal, batch rename, compare tools, full custom service
menus, per-folder view settings, regex/glob filtering, version-control integration
and advanced network/server workflows. These are later capabilities to evaluate,
including external tools/plugins, not a claim that every item is built into Dolphin.

Explicit anti-goals: a separate file-manager application; reimplementing file
semantics; cloning Dolphin's desktop chrome; permanent menu bar, icon toolbar,
status bar, tiny zoom slider or split-view affordance; default auxiliary panels;
right-click-only actions; permanent file-category/action pills around Just Type;
new recency/learning machinery or a virtual-keyboard project hidden in this scope.

### Active / Ambient integration

Active is the working surface for browsing and file actions. Ambient/Card Line
presentation stays quiet and retains the established card form; it must not
expose the full browser's controls at reduced size. Negotiate Active geometry
through a versioned Kadunce guest contract, retaining standalone fallback when
Kadunce is absent or incompatible. Do not locally resize a Card Line-only lease
or treat Tette's layer-shell guest as a normal application window.

Preserve query, browse location/history, selection, scroll and appropriate focus
through expansion/contraction. Record why expansion occurred: provisional rule
is to contract on browse exit only when browsing caused expansion; manual Active
persists until explicit collapse. Validate this rule before implementation.
Back/Escape dismisses a contextual action first, then exits the content mode;
respect the shared Meta toggle and guest cleanup contract.

Current Tette process exits on launch/dismissal. File jobs cannot simply inherit
that lifecycle. Feasibility must select a bounded job owner and explicit behavior
for hiding, launching another app, guest loss and process exit: either proven
handoff/continued ownership or a visible finish/cancel decision. Ambient transition
alone must not kill a copy or orphan an authentication/conflict prompt. Do not add
an always-on service without evidence that existing KDE job facilities cannot fit.

### Phased production plan and gates

| Phase | Bounded work / owner | Exit evidence |
| --- | --- | --- |
| F0 Backend feasibility | Tette backend: inspect installed Qt/KF/KIO versions, reusable models/actions, QML/widget boundaries, network workers, previews, undo and job lifetime | Disposable-directory spike lists local/unindexed/remote locations, opens a file, copies/trashes/restores, handles a collision, device removal and dismissal during a job; document gaps and supported dependencies before committing architecture |
| F1 UX/state prototype | Tette UI + Kadunce guest contract design; ready/results/selected/browser/operation/error states in existing black card | Touch and keyboard walkthrough of per-file actions, Places/path navigation, both browse entries at Active sizing, Ambient return, focus/scroll retention and contextual-only chrome; resolve All files scope naming and collapse policy |
| F2 Read-only browser MVP | Tette adapter/UI: Places, directories, breadcrumbs/editable path/history, previews, hidden/sort/filter/current-location search; Just Type selection, Open/Open With and Show in folder | Actual unindexed browsing, stable identity after late results/removal, empty/denied/offline states, large-folder responsiveness, correct MIME launch and usable touch targets; explicitly not full V1 yet |
| F3 File-management V1 | Tette backend/UI: remaining must-have mutations, multi-select, clipboard/drop, Trash/restore, properties/permissions, devices/network and job/conflict UI | Disposable-file operation matrix passes, including name collisions, symlinks, read-only targets, cancellation/partial failure, busy/unplugged devices and supported remote destinations; prove job lifecycle across hide/launch/guest loss |
| F4 Integration and polish | Tette + Kadunce, Temperance only where existing progress/notification contracts apply | Physical Z13 touch plus mouse/keyboard, scaling/rotation/display changes, Active/Ambient transitions, accessibility/focus/IME, drag versus card-swipe arbitration, E1/E4/E5/E6 regressions, no permanent chrome growth |
| F5 Production packaging | Respective repository owners, through existing release gates | Reproducible dependencies/build, bounded memory/preview/search work, install/uninstall and fallback documentation, source versus installed provenance and user physical acceptance; publish/install only under the applicable authorization |

Next bounded feature task when this queue resumes: F0 feasibility report and
throwaway probe, not the full browser implementation. Keep existing production
blockers first. Planning append only: no source behavior, installed binary,
release status or physical acceptance changed by this entry.
