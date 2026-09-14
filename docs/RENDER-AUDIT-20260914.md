# Accepted rendering freeze — September 14

## Current follow-up: source bounds

J passed tilt70f3145, then installed source-bounds e59f5b6: no reproduced
stretching and visibly consistent Card Line sizing. Freeze/push authorized.
Installed exact hash and live safety switch verified. Previous main:6c150c4.
J found Zen stretched unstacked; Active clears it. Journal timeline: 11:19:11
Bento entry, 11:19:23 restored Bento to Card Line, screenshot around11:19;
11:21:50 Zen Active recovered. No texture dimensions were logged at failure.
This points to restoration, not proof of load or a specific buffer race.

Accepted installed e59f5b62ecb8f9d1c122ece2db03e1d9d62a3267584eb15be73e28a440ef29b9
preserves tilt sampling. Before visible or hidden-neighbor redirection, compare
frame size and buffer/expanded rectangles relative to frame origin. Changed bounds
discard only that redirected source. Unchanged positions do not recapture.
Native/Active paint removes metadata; window deletion removes it. Metadata holds
three rectangles per seen live window, no pixels. Log changed bounds only, not
each frame. No timer, focus/geometry writes or readiness delay.

KWin OffscreenData watches damage and reallocates on texture-size changes, but
has no separate source-mapping key. This correction addresses that gap; Zen's
original root cause remains unproven despite physical acceptance. No full-output
repaint loop or snapshot cache. Installer ../install-kadunce-source-bounds-20260914.sh;
--rollback restores accepted tilt70f3145. Production/control passed; private runtime
evidence /tmp/kadunce-unload-test.daLIiK. Accepted for publication.

J: use Zen/PWAs normally; optionally Bento then Card Line without activating Zen.
If stretching recurs, leave it present and inspect the source-bounds log rather
than asking for repeated reproduction. Black-bar sizing is unchanged.

## Tilt trial history

Accepted base: main6c150c4, installed eac125d Bento motion. J clarified that
Zen is normal UNSTACKED; the stacked view introduces wavy search-bar edges.
Do not describe the plain stack capture as passing.

Local KWin6.7.5 OffscreenData::maybeRender uses live expanded window geometry,
output scale and GL_LINEAR filtering, not a retained fullscreen snapshot.
Kadunce's uniform contain scaling then applies rigid fan rotation. Its fragment
shader previously sampled that texture once. Rotation/downscaling aliasing is
a plausible cause, not established by screenshot alone.

Bounded trial: four bilinear samples at quarter-pixel derivative offsets only
when the paint rotation is nonzero. Unrotated cards keep their existing lookup.
No source resizing, capture/cache, geometry, ownership or input changes. This
may soften text and costs extra texture reads on tilted cards; physical comparison
must establish whether it helps. Do not stack more filtering fixes on a failure.

Candidate SHA256:
70f314575f14a73289ef04d2e363d3234ade38d38d9321f44f3895edddc43fad
Rollback SHA256:
eac125d443fd513f50aeb7da6a25483b920ba8d50a0ed9422f1911238c07e399
Installer: ../install-kadunce-tilt-sampling-20260914.sh (--rollback supported).
Not installed, accepted or pushed. Production build passed. Private runtime
smoke passed: /tmp/kadunce-unload-test.qrjwCW. Control and pinned installer/live
safety checks passed. This is not visual proof.

Separate black-bar finding: fixed backing plus KeepAspectRatio necessarily
letterboxes nonmatching live window shapes. Idle time cannot fix that mismatch.
Fullscreen sources would require native client resize/redraw and can disturb
games/restoration. Before changing policy, distinguish genuine aspect margins
from bad bounds; compare native frame/expanded bounds and fitted content rectangle.
Do not restore rejected snapshot/readiness machinery. No black-bar fix in trial.

J test: compare Zen search-bar straightness stacked versus unstacked, inspect
text softness, and cycle/grab a stack for responsiveness. If unchanged/worse,
return to exact accepted build and review evidence before another approach.
