# Bento divider grab candidate

Accepted local freeze:0184129; installed rollback SHA256
cd7df839bcc8fe071fb54876de37c14a873d12a9b603cba7ab9205a50d9a8fb1.
Candidate SHA256 cd34ba2c1d5b5f062ee8679dc060e987e30abb899cdc5128cbabd62e72e31a97.
Installer: ../install-kadunce-bento-rail-20260914.sh (--rollback restores accepted binary).
Not installed or pushed by agent.

## Behavior

- Rounded42x4 divider pills (48 while held);32px cross-axis grab target.
- Pointer/touch moves a clamped outline preview; release sizes connected panes once.
- Cancel/multitouch/Esc leave the original layout; unload never waits for a held rail.
- No rail through covering popup/free window. Other-output Card Line does not hide monitor rails.
- Reuses connected-rail solver; native minimums now include14px gap allowance.
- Native carry adoption retires its previously forwarded router contact, since carry
  consumes the eventual release. This prevents stale held state blocking subsequent rail input.

## Evidence

Final candidate private evidence: /tmp/kadunce-unload-test.OTvrfw/session.log.
All six checks pass, including unloading while a rail touch is held. Installer
--check verified both binary hashes and the live safety switch without installation.

Build and ctest carry-input-route/bento-transfer/panel-input/bento-layout passed;
verify-control passed. Private v5l8uD passed pointer/touch preview-only resize,
release/cancel and existing side/share, singleton, launch and overflow tests.
First private rail attempts0dIGcp/usyfG4 stopped before resize because stale forwarded
contact state prevented pickup; fixed at explicit native-carry adoption, not by
loosening native geometry or ignoring input ownership.

J test: resize via pill with finger, release; repeat reverse direction with mouse;
verify ordinary snap/overflow and bottom Card Line swipe still behave as accepted.
