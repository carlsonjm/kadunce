# Current state

September14 accepted freeze: output-local Active bottom clearance matches Bento's extra
10px only when MaximizeArea reserves bottom space; dock-free output keeps symmetric
gutters. No Card Line/native ownership or repaint changes. Production build,
layout/carry-paint and independent control pass. J installed and passed connected
displays and unplugged tablet-only. Installed211f7c2 and live safety verified;
freeze/push authorized. Ghostty's reported stray1px bottom line/flat-dock trigger
remains a separate unconfirmed-cause follow-up; no speculative rendering change.
Evidence, installer and tablet/monitor physical checklist:
../../tettegouche/docs/ACTIVE-DOCK-20260914.md. Preserve unrelated roadmap edits.

J accepted installed Tette drawer bridgebec4d8 and authorized freeze/push.
Dock-safe bounds,220ms neighbor departure/return and guest-lifetime suspension of
stack elevation/raising passed. Protocol3 retained; no card geometry/order changes.
Focused model, private guestxvnGsl, control/live safety pass; installed hash verified.
Tette finalb681ec is also installed/J-passed. Installer/rollback and evidence:
../../tettegouche/docs/ACTIVE-DRAWER-20260914.md. Monitor expansion remains deferred.

## September 14 — Accepted Bento column/full-divider freeze

J passed installed overflowcd7df, frozen at0184129. This accepted batch includes
membership/minimize reflow, side/share intent,
minimum-aware fitting subsets with minimized overflow and required visible launches.
See BENTO-SIDE-20260914.md. Immediate-unload visibility exception remains approved.

J passed installed railcd34ba in both axes and column splittingcf5011. It preserves an
occupied edge column and splits top/bottom, absorbing a vacated third column into
its remaining neighbor. Small columns on tablet; larger columns also on monitor.
Minimum failures retain existing fitting/overflow fallback. Rails now hide at idle;
90ms stationary hold reveals preview, release applies once. Early motion cancels
activation. Native carry adoption retires forwarded contacts; no new resize loop.
Fullrail302b1 extends capture across the full shared divider, not just the pill;
Plasma receives no initial divider press. Outside desktop behavior is unchanged.
Private NYkGoz/l96RD3 pass off-center horizontal/vertical grabs, cancellation,
split/overflow and held unload. Control/live safety/hash checks pass.
Installer ../install-kadunce-bento-fullrail-20260914.sh; rollbackcf5011.
J passed installed302b1 and authorized freeze/push. Installed hash and live safety
verified. See FREEZE-20260914-BENTO.md. Monitor hardware acceptance remains deferred.
Installed, J-accepted Bento motion eac125d adds shared220ms ease-out reflow,
with native placement once and paint-only interpolation. Fresh input interrupts;
no snapshot/cache or resize loop. Focused layout/transfer, control and private
column/rail motion lifecycle checks pass (X4rEyt). Installed hash/live safety verified.
Installer ../install-kadunce-bento-motion-20260914.sh; rollback accepted302b1.
See BENTO-MOTION-20260914.md. Rendering e59f5b6 installed/J-passed; hash/live
safety verified, freeze/push authorized. Includes tilt sampling and bounds
invalidation. See RENDER-AUDIT-20260914.md. Black-bar policy unchanged.

## September 14 — Accepted Active ownership lifetime freeze

J accepted this freeze. Active departure parks
per-window desktop restore records instead of applying them. Reentry reuses them;
explicit release/unload restores them. Bento collects all retained records before
Card Stage releases. Committed departure forgets only the transferred record.
Native resize requests on retained cards are cancelled without releasing ownership.
No renderer, animation, paging threshold or dock hit-region changes.

Production compiles; five focused model/input tests and verify-control pass.
Private runtime lifetime and direct-edge gesture checks pass (uysp1k evidence).
See tests/unload-probe/lifetime-runtime-session.sh; its fixture requires Virtual-0
as tablet and direct system edges enabled. Never install that fixture binary.
Installed production8e4c8436 verified; live safety switch passes after installation.
Exact hashes/rollback: FREEZE-20260914-ACTIVE-LIFETIME.md.
Next: deferred monitor handoff acceptance when hardware is available.

## Card Line motion batch — September 13

Accepted historical row build/rollback: FREEZE-20260913-ROW-MOTION.md.

Ordinary and held row paging use interruptible220ms presentation, immediate
selection, captured reversal and shared center/neighbor displacement. Wrapping
uses the same progress rather than a separate half-clock; single-card gutters
remain stable. Existing fan endpoints and contact-anchored44% held geometry stay.
Departing cards are paint-only, not input/model-visible; release retires the old
detached-row transition. Stack timing/order and native ownership are unchanged.

At most two next-hidden selected faces are prepared with KWin OffscreenEffect,
one per frame and32MiB per extra surface. Empty final clip prevents screen output.
Neighborhood/Active/release cleanup retires hidden preparation; finite frames
prepare both sides. No new retained snapshots, background timer, input readiness
gate or native geometry. Paging can outrun preparation and never waits for it.
Entry is opaque; clipping replaces the superseded16px/fade experiments.

Historical row-motion rollback: ../install-kadunce-row-unison-20260913.sh --rollback.
Source/runtime checks and J's physical pass cover this scoped freeze, not all
hardware. Zen sampling remains open; do not restart caching experiments.
Preserve unrelated NEXT-ROADMAP edits.

## Accepted QoL freeze

September13: shared enterActive now clears temporary Card Line elevation after
publishing Active. Shortcut toggle already did this afterward, but timed arrival
and external activation paths could retain elevation and cover Plasma popups.
No geometry, ownership, timing, renderer or input policy changed. Source guard
checks the shared boundary; build, control and live-control checks pass.
Local ../install-qol-20260913.sh bundles this with Tette Meta toggle/panel focus;
--rollback restores the accepted ae40 plugin and both prior Tette binaries.
J reports a full physical pass and authorizes freeze/publication to main.
Accepted QoL plugin hash (now rollback):
6c981b08cb951f26e954bc6007d66998b7b8887f701881eb0bebd7b9dae47368.
All14 native tests pass; independent control2/2 and live safety checks pass.
No Temperance source edit is needed for this specific elevation leak.

Updated September13. Accepted post-paint stack baseline frozen for main;
supersedes the earlier contact-driven freeze20f2007. See
FREEZE-20260913-STACK-BASELINE.md. Trusted repair is not promoted.

## Previous baseline provenance

Historical rigid-fan/caching experiment provenance and exact rollback hashes:
FREEZE-20260913-STACK-BASELINE.md. Rejected caching experiments stay rejected.

Relative to the preceding rigid-fan baseline, only runtime changes are
Effect.cpp/.h: prePaintScreen records whether animation
or drop settling needs continuation; postPaintScreen requests the next repaint
after KWin consumes current layer damage. Pre-paint masks remain unchanged.
The pre-paint latch ensures a final endpoint frame if the animation expires
during painting; the next inactive pre-paint clears it. No idle repaint loop,
new timer, cache, timing/geometry/ownership/input changes.

## Preserved accepted behavior

Contact-anchored44% held size;500ms shoulder paging;300/350ms physical-edge paging;
inward cancellation; canonical insertion depth/selected-face continuity;
rigid browse/insertion fan; outline-only seam; native ownership and dock safety.
Live proportional previews with the firm-pass backing and rounded aperture.
No retained Active image cache, readiness gates, startup shader preparation,
extra full-output damage repair or diagnostic timing hooks.

## Evidence and uncertainty

Historical measurements and repaint ordering: LIVE-LAG-EVIDENCE-20260913.md.
Accepted post-paint baseline; not an instrumented GPU root-cause proof.

Power/refresh/runtime contribution to historical lag remains unproven.

## Safety and next bounded action

A showfps + Spectacle diagnostic crashed KWin at16:14:01 (screencast framebuffer
trace), then Zen. Witness result INVALID. No intentional restart/app close.
Recovered session, ChatGPT focused, showfps absent, tray kill switch verified.
Do not repeat overlay+recording on the live desktop.

Next: ownership/drop block in NEXT-ROADMAP.md. No speculative caching/power tuning.
Zen sampling remains separate. Trusted repair is not promoted.

## Recovery and other scope

Local candidates remain under ../work, outside Git; fresh clones use install.sh.
Trusted repair stays27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9.
KWin6.7.5-1.2 engine patch unchanged. Never reinstate rejected rough-swipeba47bf.
Occupied-tablet arrival considered solved; Affinity splash deferred.
B owns Temperance. Preserve unrelated NEXT-ROADMAP edits.
