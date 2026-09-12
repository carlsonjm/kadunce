# Display handoff candidate — 2026-09-10

Not frozen or pushed. Live extended-display validation is required.

The reported disappearing item was clarified to be the dragged window, not the
cursor. The tablet paint gate hid incoming windows lacking Card Line admission.
The Effects-order input filter also preceded KWin's InteractiveMoveResize filter
and swallowed motion over Card Line. No cursor override is needed or retained.

Changes:
- Native move/resize input passes to KWin, including motion, release, and wheel.
- Tette outside-click capture starts only on the tablet, matching touch policy.
- One explicitly carried native app window uses normal output-clipped rendering
  and elevation. Passive neighbors still cannot paint on external outputs.
- Ordinary cross-output moves are admitted after KWin completes the move; Bento
  retains its own ownership. Completed output assignment, not pointer position,
  determines acceptance, including canceled moves.
- Incoming transfers reuse app arrival insertion/settling. The former appendCard
  already selected a new card; the mismatch was ordering/animation, not failure
  to select newly appended cards.

Seven native tests pass, including new paint-routing and input-ownership checks.
Source and safety-control checks pass. These do not replace live KWin rendering
and asynchronous Wayland move validation.

Live checks: drag an ordinary external window across the tablet seam, drop into
Card Line with and without Tette, cancel with Escape, drag back out, and check
the monitor never displays passive tablet neighbors. Repeat a Bento transfer.
Check native resize, panel clicks, Tette outside taps and swipe dismissal.

Still pending: destination feedback for Kadunce's lifted-card gesture, broader
disconnect/reconnect reconciliation, and motion polish. Native carried windows
have live visual feedback; lifted cards are a separate input transaction.

Separate machine diagnosis: Z13 desktop posture explicitly disabled the ELAN
touchscreen. User requested always-on touch; persisted tablet settings now set
disable_touchscreen_in_desktop=false. KWin's touchscreen enabled property was
verified true. No Kadunce touch mapping or desktop folder settings changed.
