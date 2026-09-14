# Current state

## Accepted stack polish freeze

J accepted reverse browse, held ordering and equal neighbor spacing; freeze/push
authorized. Installed artifact verified against candidate:
fc3126e8b0864c13cbc12548b264f7e979f385fcf50863ee212e412040e22efe.
The accepted reverse browse
leaves upward motion exact; downward accents the incoming face downward while
the outgoing face only interpolates to its fan slot (no second send-away bump).
Build, directional motion/source/control and installer checks pass; J accepted.
Stack browse uses220ms rigid accents, at most24px/0.25degree, zero endpoint
value/slope, captured interruptions and immediate selection. Input stays unchanged.
See FREEZE-20260913-STACK-BROWSE.md for scope and separate held-order bug.
../install-kadunce-stack-browse-20260913.sh --rollback restores exacta422853a.
Held-order build passed J's physical tests:
detach now exposes the preceding visual shoulder; pickup and held paging refresh
destination back-to-front stacking while the held card remains elevated.
Build, five focused tests, source/control checks pass. Four-stack tests cover every
lift position and cancellation. No motion/input/cache changes. Candidate282e7ef6;
../install-kadunce-held-order-20260913.sh retains installed15882014 as rollback.
Neighbor revision preserves the compact inward edge/gap, sharing40% of the visible
side peek equally between shoulders (two for3 cards, three for4+). Selected-relative
depth preserves ordering. Centered browse/insertion and timing stay unchanged.
Headless geometry checks cover both sides/counts. J's physical acceptance passed.
../install-kadunce-even-neighbor-20260913.sh is opt-in; rollback restores70fdce52.
Unrelated NEXT-ROADMAP edits preserved.

## Card Line motion batch — September 13

J reports the installed unified-row build passed and authorizes freeze/push.
Installed plugin verified:
a422853a94c2b0668e8f96c12aaa3940e0a128e24b680a774385863dcedfe36b.
See FREEZE-20260913-ROW-MOTION.md for provenance and exact rollback.

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

Previous main38753bf is the QoL baseline below. Exact immediate rollback8b766377
(preparation pass J called V GOOD) is preserved by
../install-kadunce-row-unison-20260913.sh --rollback.
Source/runtime checks and J's physical pass cover this scoped freeze, not all
hardware. Zen sampling remains open; do not restart caching experiments.
Next bounded motion scope, only on request: stack cycling and pickup/release.
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

Previous baseline c063a199850224fbc7ed364fadc9be6fd4f53bb5fadef14a8eb88ff323022906
is the exact rigid-fan artifact J called “firm, passed with flying colors.”
Later retained-previewad870751 received the qualified pass with initial lag.
Backing-removal587591 was ineffective and is rejected; its source change reversed.

Previous accepted post-paint baseline (now the QoL rollback):
ae40b6e43919f944b4f4664735d488f7fc3ccd280457c461f1a55a933d905d6d
../work/kadunce-postpaint-20260913
../install-kadunce-postpaint.sh
--rollback restores exact c063. Nothing rebuilt in KWin itself.

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

Read LIVE-LAG-EVIDENCE-20260913.md. Controlled touch selection coincided with
crossing38px (~71ms into the scripted swipe). Six shortcut selections took7–9ms.
Without recording,124 KWin D-Bus samples median5.87ms/max11.62ms while transition
coordinates advanced. Recorded frame gaps are confounded by screencast overhead;
not proof of GPU stalls.

KWin6.7.5 calls prePaint before resetting current layer repaint state.
Its own SlideEffect requests continuation in postPaintScreen.
Source regression guard fails before correction and passes after it.
Plugin build, source guards, control2/2, live tray and diff checks pass.
J's post-install physical feedback: still smooth; accepts this as working baseline.
Zen's rendering artifact remains open. Live safety switch verified after install.
This is user acceptance, not an instrumented frame-time or root-cause proof.

J reports major lag upstairs on battery; plugging in downstairs became smooth,
and remained smooth after unplugging. Power/refresh/runtime state is a plausible
factor, not diagnosed. Do not claim this patch conclusively explains that change.

## Safety and next bounded action

A showfps + Spectacle diagnostic crashed KWin at16:14:01 (screencast framebuffer
trace), then Zen. Witness result INVALID. No intentional restart/app close.
Recovered session, ChatGPT focused, showfps absent, tray kill switch verified.
Do not repeat overlay+recording on the live desktop.

Next: preserve this accepted working baseline; no further speculative caching or
power tuning. Animation polish can be scoped separately. Zen sampling waves
remain separate/unproven. This freeze does not promote trusted repair.

## Recovery and other scope

Local candidate binaries/source archives are retained under ../work; these are
not shipped in Git. Fresh clones use the repository's standard install.sh.
Trusted repair stays27f775ecad1e2d132f985950660c8d039eaf015b7e499723cf348cd51c4fa1d9.
KWin6.7.5-1.2 engine patch unchanged. Never reinstate rejected rough-swipeba47bf.
Occupied-tablet arrival considered solved; Affinity splash deferred.
B owns Temperance. Preserve unrelated NEXT-ROADMAP edits.
