# Active ownership lifetime — September 14, 2026

J: “That's an easy pass, freeze and push.” Base:98f28c0.

Active geometry survives Card Line and selection changes. Per-window original
restore records persist until explicit release/unload or committed transfer.
Bento collects retained records before source release. Native resize requests
cannot silently release retained cards. No motion, rendering, paging thresholds
or dock hit-region changes.

## Evidence and boundary

- Production build and five focused model/input tests passed.
- verify-control passed; post-install verify-live-control passed September14.
- Private direct-edge/tablet fixture: /tmp/kadunce-unload-test.uysp1k/session.log.
  Five checks: repeated Active/Line geometry, rejected native resize, bottom
  swipe, second-app/Bento restore continuity, and unload from Card Line.
- Fixture overrides were removed before rebuilding production.
- J passed tablet bottom swipe, app switching and release/Bento round trip.
- Physical monitor handoff remains deferred; this freeze does not claim that pass.

## Local deployment and recovery

Installer: ../install-kadunce-active-lifetime-20260914.sh
Bundle: ../work/kadunce-active-lifetime-20260914/{candidate,stable}.so
These are local deployment artifacts, not committed binaries.

Installed production SHA-256:
`8e4c843636122efc73f48ceda261f35fdfcc072955e91d324386eaee70b8d55a`

Accepted pickup rollback SHA-256:
`a7482cc840526c86428f79bec1a70db68749a2241b94ecbca604eeeb0a8336d8`

Use the installer with --rollback and save/log out/in to load that prior build.
No KWin repair, compositor replacement, or session restart is bundled.
