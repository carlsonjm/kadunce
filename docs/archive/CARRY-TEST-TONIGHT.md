# Integrated carry — first physical test

This is an opt-in test candidate, not a final release. The accepted build and
trusted repair archive remain the recovery baseline. Installation must not restart
Plasma/KWin or promote repair. Save work, install, then log out/in yourself.

Install from the parent workspace: `bash install-kadunce-carry-test.sh`.

Rollback: the same command with `--rollback`, then log out/in. This restores the
accepted stability binary directly; it does not rebuild or replace trusted repair.

## Couch pass (about 5–10 minutes)

1. **Safety first:** verify the Kadunce tray control is present. Disable/re-enable
   once. Then hold a card with touch and disable using the mouse; releasing the
   finger must do nothing. Re-enable and confirm a fresh tap works.
2. **Card Line:** use two ordinary apps, then add a third. Check center/neighbor
   sizes and both directions. Try a quick flick, slow swipe, hold/drag and canceled
   drag. A finger-held card should follow both axes. Record roughness; timings have
   not been declared physically accepted or retuned in this candidate.
3. **Stacks:** join cards from either side, step through insertion slots, cancel a
   lift and browse the stack. Check selected identity/order and interrupted motion.
   Native Active/Bento carries do **not yet** select an incoming stack slot; test
   stack insertion from Card Line only.
4. **Tette:** open, outside-tap to dismiss, reopen and swipe away. Card Line should
   stay open after the swipe. Launch an app and check center replacement/Active.
5. **Release:** small, maximized and true-fullscreen window → Card Line → Active
   → Ctrl+Esc. Check gutters, restored desktop state, live content and corners.

Stop on a missing safety switch, black/invisible window, stuck contact, incorrect
restoration or repeated rough quick-swipes. Disable immediately; do not run repair
or keep layering fixes on top of a failed physical result. Record the action/app
and whether a mouse or finger was used. Roll back the binary and log out/in if
needed. Physical feel, not virtual-test success, decides acceptance.

## Monitor pass (can remain parked)

- Ordinary monitor window → top/left/right edge: Bento forms and recruits eligible
  windows on that display. Back away before release: it stays an ordinary window.
- An open-space drop joins an existing Bento; otherwise it remains a normal window.
- Exchange existing Bento panes; check restored original windows after disable.
- Carry Active/Bento/ordinary monitor windows between displays; only the held
  window crosses the tablet boundary. Passive neighbors never leak externally.
- Check bottom dock controls, fractional scale and unplug/reconnect separately.

## Known unfinished work

Native-to-stack atomic admission, displaced-neighbor animation, and additional
tablet arrival/receiver feedback remain. These do not become implicit passes by
installing this candidate. No rejected quick-swipe tuning has been reinstated.
