# U1 — source ownership audit

September 11, 2026. Main a74991f plus existing local refactor/handoff changes.
Read-only source audit, not a live reproduction or new snapping implementation.

| Current path | Source evidence | Unified-carry gap |
| --- | --- | --- |
| Lift | CardStageController::beginCardGrab clears transition and detaches selected stack member | Preserve displayed pose/anchor and source rollback before presentation changes |
| Mouse hold | WorkspaceInputRouter::pointerMotion updates destination and horizontal offset | Needs shared 2D pose, not mouse-only destination tracking |
| Touch hold | touchMotion updates horizontal offset/edge/stack timers, not updateCardGrabDestination | Mouse/touch transfer semantics differ |
| Horizontal leash | updateCardGrab clamps displacement to ±1.10 pitches | Renderer-only change cannot free carry |
| Edge paging | updateEdgePaging/pageCardGrab and dwell timer | Would compete with new snap edges |
| Stack order | updateStackInsertion uses center-card 34%/66% zones and insertion timer | Needs destination-local gap geometry |
| Stack end dwell | Insertion timer clears stack target then pages row | Implicit meaning switch must not race layout placement |
| Ordinary drop | finishCardGrab may reorder at ±0.82 pitch or commit a detached member after movement | Invalid drop now requires restoration unless explicit destination accepts |
| Card→monitor | finishCardGrabOnOutput removes source, moves window, then asks Desktop Stage to admit; native fallback counts as success | Destination-first transaction is NOT enforced |
| Bento→output | DesktopStageController::handoffWindowToOutput removes source before adding destination | Needs prepare/accept/commit with rollback |
| Native titlebar move | Effect records m_nativeCarry; handleManualWindowChange releases Active | Native owner differs from card-hold owner; adoption needs an explicit boundary |
| Output fence | Effect::paintWindow/cardPaintRoute exempts native carrier | Add narrowly scoped visual-carrier handling; never expose passive neighbors |
| Dock/tray | isPanelPoint includes visible Dock/AppletPopup | Preserve held-touch safety-control access |
| Tette | Separate guest lease/forwarded input | Preserve tap/swipe and generation; guest is not normal membership |

Files: native/src/WorkspaceInputRouter.cpp, CardStageController.cpp,
DesktopStageController.cpp, Effect.cpp, DisplayHandoffPolicy.h,
CardWorkspaceState.h (repository-relative navigation references).

## Architecture correction

ARCHITECTURE.md's accept-before-source-removal sequence is a **target contract**,
not proof of implementation. Both transfer paths above mutate the source early.
Do not assume atomic handoff from the prose or weaken rollback on that basis.

## Rules to formalize next

- Native remains native until adopted. No global quick-tile setting changes;
  quickTileModeChanged callbacks alone are not snap arbitration.
- One owned carry has one preview and generation. Visual output crossing does
  not require premature physical-window ownership changes.
- Preview is read-only relative to committed membership. Stack gaps bind to
  target identity/revision so closure cannot silently shift a pending index.
- Prefer synchronous acceptance/commit; later native configure completion still
  needs lifetime/output checks. Preview validity isn't geometry acceptance.
- Cancel restores order/focus. If source output/window has vanished, use an
  explicit surviving-output fallback, never stale pointers or invented state.
- Safety cancellation overrides the gesture, then drains its release.

Next packet: headless carry/destination contract and tests for duplicate release,
rejection, target revision changes, closure, output loss, cancellation and device
parity. No native setters, renderer changes, timing changes or install in that
packet. Carry-browsing and companion UI remain later decision checkpoints.
