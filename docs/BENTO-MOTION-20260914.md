# Accepted Bento reflow motion freeze

September 14: J passed the installed build and authorized freeze/push.
Installed SHA and live safety switch verified. Previous main is 733543f;
rollback is the accepted full-divider build.

Native layout commits once. Changed visible panes share a 220ms OutCubic
presentation transition using the existing rounded live paint path. Same-output
retargeting captures the current visual pose before native writes. Fresh input,
release and unload cancel motion; output/target mismatch retires it. New arrivals
and carried cards do not animate from invalid source geometry. No new snapshots,
cache, hold delays, native resize loop, layout policy or ownership changes.

Candidate SHA256:
eac125d443fd513f50aeb7da6a25483b920ba8d50a0ed9422f1911238c07e399
Rollback SHA256:
302b1cedf851c4015f171311cd7e6e5cfc44a2d042d16923af9c34bbdf00b907

Installer: ../install-kadunce-bento-motion-20260914.sh; --rollback restores
the accepted binary. Save work and log out/in after installation.

Evidence: production build, bento-layout/bento-transfer and verify-control pass.
Private /tmp/kadunce-unload-test.X4rEyt passes column split, quick-hold rejection,
off-center rail resize and asserts a multi-pane transition starts and expires.
This is not a frame-time measurement or replacement for physical acceptance.

J passed: divider resize, minimize/restore and new input during motion.
Monitor acceptance remains deferred; Zen sampling is separate.

Next rendering audit: J clarified Zen is normal unstacked, but stacked tilt
distorts the search bar. Other windows can show oversized black bars despite
idle time. Fullscreen-source capture is a design proposal, not an implemented fix.
