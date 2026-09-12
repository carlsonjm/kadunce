# Architecture-heavy work remaining

September 12 checkpoint. These are ownership/transaction decisions, not a claim
that only one model can implement them. Do not expand a packet into the next one.

1. **Occupied-tablet admission** (NEXT-ROADMAP P1). One transaction reserves the
   incoming window and existing Active/Card Line/Bento residents, preserving each
   original restore record. No fighting geometry writers or monitor bounce.
   Gate: ordinary/card/Bento sources; open versus edge; cancel, close, unplug,
   minimum-size rejection and held disable. Finish and physically accept first.
2. **New-window lifecycle admission.** A new eligible app on a display already
   hosting Bento joins it, including single-card Active. Distinguish launch/splash,
   transient dialogs and real windows; don't hijack unrelated display launches.
   Gate: Ghostty launch, slow Affinity splash, external/Steam launcher chains,
   window ready late, focus/output changes and close-before-ready.
3. **Native carry into ordered stacks** (U1). Reuse the existing versioned stack
   insertion plan. Complete N+1 slots from either side, cancellation and target
   replacement. Do not introduce another membership system or motion controller.
4. **Input/motion convergence** (U2). Audit remaining Card Line router versus
   native carry boundaries. Fix regrab/flick/hold/settle continuity from measured
   presented poses, without a blanket timing rewrite. Only the input ownership
   and interruption decisions are heavy work; isolated visual tuning is not.
5. **Ecosystem focus/guest contracts** (E2/E4/E12, after production acceptance).
   Fullscreen Meta/dock access, Tette Active/file-drag behavior and focus return
   need one agreed lifetime/output contract. Separate future packet, not a
   prerequisite to physically accepting today's window refinements.

Before freezing: P3 real-device regression, fullscreen/maximize/minimize,
monitor removal/rotation and independent kill-switch/repair verification.
Approval to commit/push or promote repair is separate from test acceptance.

## Lighter-work lane now

Bounded tests and reproducible bug capture, documentation/artwork/packaging,
search presentation, banner sizing, and isolated animation tuning after its
contract is fixed. Each packet names files, acceptance tests and stop conditions.
Notifications-to-stock handback remains coordination-sensitive, not merely visual.
Persistent stacks/service and further renderer extraction are optional structural
decisions; they must not silently extend the production-fix milestone.
