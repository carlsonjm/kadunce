# Current state

## Accepted QoL freeze

September13: shared enterActive now clears temporary Card Line elevation after
publishing Active. Shortcut toggle already did this afterward, but timed arrival
and external activation paths could retain elevation and cover Plasma popups.
No geometry, ownership, timing, renderer or input policy changed. Source guard
checks the shared boundary; build, control and live-control checks pass.
Local ../install-qol-20260913.sh bundles this with Tette Meta toggle/panel focus;
--rollback restores the accepted ae40 plugin and both prior Tette binaries.
J reports a full physical pass and authorizes freeze/publication to main.
Installed plugin hash verified:
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
