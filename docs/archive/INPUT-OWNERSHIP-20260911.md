# Input ownership — Block 3

September 11, 2026. Local candidate only; no install or live gesture acceptance.

## Existing routing contract

| Sequence | Owner and termination |
| --- | --- |
| Native desktop pointer move/resize | KWin; router passes events while native interaction is reported |
| Pointer press on Plasma panel | Plasma through release, including motion over a card |
| Contact inside Tette | Tette through release, including travel outside its surface |
| Contact outside Tette on tablet | Kadunce; stationary release dismisses, movement disqualifies dismissal |
| Card Line contact | Kadunce; release resolves navigation or a grabbed-card transaction |
| Provisional bottom-edge touch | Client until deliberate single-finger upward movement and successful client cancellation |
| Foreign pointer release | Original owner; cannot activate a card without an owned press |

These rules describe current code and focused tests, not complete lifecycle
coverage. Tablet cutoff, geometry, thresholds and animation are unchanged.

## First cancellation slice

Touch cancellation clears touch contacts and a pending touch hold. A touch-owned
grab rolls back, not commits. It does not clear a pointer outside-Tette transaction
or stop pointer-owned hold/stack/edge timers. A second device cannot replace the
owner of an already-armed hold. Cancellation is consumed only if Kadunce owned
contacts and no observed contact was forwarded; mixed streams must still deliver
cancel to their client. Repeated cancellation has no additional rollback.

PanelInputTest now covers guest-only and mixed guest cancellation, independent
pointer release, canceled hold timers, exactly-once grab rollback, fresh contact
recovery, and pointer-hold survival across a second device's canceled touch.
Nine native tests, source guards, packaged control tests and the live safety check
pass. Installed plugin and trusted repair archive remain unchanged.

## Lifecycle cancellation slice

CardStageHost now explicitly invalidates router actions before stage release,
an admitted card closes, or a guest ends. Effect also invalidates them on display
removal. Pending hold/dwell timers stop and a router-owned grab rolls back once.
Consumed touch IDs and the initiating pointer button remain in a draining set:
motion/release is consumed without actions even after presentation disappears.
Forwarded contacts are preserved. A real touch-cancel clears drained touch IDs.
Normal release relinquishes ownership before dispatching an action so synchronous
lifecycle callbacks cannot leave the next click trapped in a stale drain.

Tests cover repeated invalidation, delayed holds, cross-boundary release draining,
fresh input recovery, forwarded Tette contacts, exactly-once grab rollback, and
synchronous activation invalidation. Nine native tests and source/control/live
safety checks pass. This remains uninstalled; physical lifecycle acceptance is
pending. Controller hook wiring is source-reviewed, not a live compositor test.

## Remaining Block 3 work

Update: the unload investigation below was subsequently resolved in the isolated
tests described in UNLOAD-VALIDATION.md. Physical acceptance remains pending.

- Effect unload cannot retain a release drain after destruction;
  its filter-chain handoff needs a separate compositor-level check.
- Verify these boundaries in a nested/live compositor. Fake targets do not
  prove KWin filter-chain ordering or physical mixed-device behavior.

Do not tune hold/swipe timings or resume the rejected animation candidate to
solve these ownership issues. Rendering and motion remain later blocks.

## Native takeover and competing-device slice

Input dispatch and all four dwell/hold callbacks now check native KWin
move/resize ownership. Takeover cancels pending actions/grabs before dispatch,
disarms provisional bottom-edge recognition, and passes the native pointer
transaction rather than retaining a stale pointer drain. Already-consumed touch
contacts retain inert release ownership. This is checked at event/timer boundaries,
not an immediate native-start signal subscription.

The first workspace gesture owns navigation: a competing device's workspace
press is consumed through release without actions, and wheel paging is suppressed
on the tablet during a gesture. Panel/Tette-client and external desktop routes
remain available. A remaining secondary touch cannot become a new primary merely
because the first finger lifted. This does not finish multi-button arbitration.

Tests cover native takeover before hold expiry and after lift, final native
release and fresh-click recovery, both mouse/touch competition directions, wheel
suppression during a grab, and external scrolling passthrough. Nine native tests,
source guards and control checks pass. Physical input/compositor acceptance and
the parked monitor test remain pending. No timing, geometry, install or repair
change is included.

## View transitions and pointer chords

Toggle, enterActive and successful guest entry now call the existing host
cancellation boundary before changing the view. Gesture callbacks may reenter
this boundary; the router's consumed-contact drain remains separate from actions.
Other asynchronous admission/activation paths and deferred commands still merit
the final review above; this is not an all-path proof.

Forwarded pointer delivery uses a button set instead of two booleans. Client or
Tette delivery continues until every held button releases, even across lifecycle
changes. Native pointer releases retire forwarded/panel button bookkeeping too.
Extra buttons pressed outside Tette are inert drains; their presence cannot hide
the primary button's motion or turn a moved gesture into dismissal. Non-left
card presses also retain release ownership. Tests cover both client chord release
orders, fresh-click recovery and moving outside-guest chords. Nine tests, source
guards and safety checks pass. Installed plugin and repair are unchanged.

## Queued activation and delayed arrivals

Deferred input activation now captures a weak reference to the selected window
and a generation ticket. Replacement requests supersede older ones; host input
cancellation invalidates tickets. The callback requires that same live selection,
Card Line, no guest/grab, and no native move/resize. Paging (including stack
paging), admission, explicit activation and staged arrivals now use the host
invalidation boundary. A delayed new-window callback rejects deleted candidates.

Arrival expansion rechecks tablet ownership and native move/resize before either
phase. Existing readiness retries, selected-window checks, hold durations, arrival
durations and guest-generation timers remain unchanged. Tette callbacks already
carry guest-generation checks; they were reviewed, not rewritten.

An event-loop test verifies canceled and superseded generation tickets. The
native-window identity/output guards and controller wiring are source-reviewed,
not exercised by that isolated test. Nine native tests plus source/control/live
safety checks pass. No install or repair refresh. Next is candidate validation
and unpacking the existing Plasma Mobile research before touch/motion changes.

## Consolidation check

Historical pre-unload-fix findings; see UNLOAD-VALIDATION.md for the subsequent
explicit teardown ordering change and full-effect test results.

Combined fake-target tests now exercise bottom-edge takeover with synchronous
view cancellation, paging with cancellation, subsequent hold recovery and exit
with a grabbed contact still down. Router destruction stops a pending hold timer.
These pass, but do not instantiate the live Effect/CardStageController pair.

Installed KWin's input.h documents automatic filter unregistration in the base
destructor. Effect restores active stages in its destructor body; router member
destruction occurs later, after the controller members due to declaration order.
No failing teardown case was reproduced. Reentrant restoration and delivery of
remaining contacts after filter removal are not proven by the timer test. Do not
equate auto-unregistration with safe mid-contact unload; isolated compositor
validation remains a release gate. See COUCH-VALIDATION.md for the prepared,
not-yet-run physical checklist. No runtime changes were made in this check pass.
