# Accepted-behavior regression gate

Refactoring may change private names and structure. It must preserve these product
assertions and the authority boundaries in `ARCHITECTURE.md`.

## Coverage map

| Contract | Automated evidence | Live evidence still required |
| --- | --- | --- |
| Active gutter is output-relative and bounded 6–48 px, default 10 px | layout and window-handling tests | real minimum-size applications |
| Release restores desktop state, including maximize/fullscreen | window-handling and restoration tests | live repaint, focus, and original geometry |
| Two groups use 64% center/54% shoulder; three or more use 54% | layout/model tests | animated continuity and clipping |
| Stacks preserve membership order and selected face | model, insertion, browse, and cancellation tests | real identity across lifecycle |
| Passive tablet neighbors never paint on another output | paint-route tests and source guards | fractional-scale multi-output clipping |
| Native move and panel input retain ownership | panel/input route tests | physical KWin and touchscreen ordering |
| Guest outside-tap dismisses; movement is not a tap | guest input tests | real guest closure and swipe collapse |
| New application replaces the guest center before Active | arrival and launch-identity tests | splash/main-window and focus lifecycle |
| Persistent disable control survives independently | control, live-control, and repair tests | authorized live toggle/re-enable |

Source guards are weaker than runtime tests. Private compositor tests are not physical
acceptance. A skipped live or infrastructure check stays unverified.

## Bounded physical checklist

Choose the smallest subset touched by the candidate and record source/installed
identity, session, display setup, and PASS/FAIL/NOT RUN.

1. Active/Card Line entry and release with ordinary, maximized, and fullscreen
   windows; verify focus, interaction, and restored geometry.
2. Card Line paging, arrival/removal, stack browsing, lift/cancel, extraction, and
   stable selected identity.
3. Guest outside tap, movement cancellation, swipe collapse, relaunch, and app
   replacement.
4. Panel controls and application input remain native; deliberate system-edge entry
   still works.
5. Cross-output carry, Escape return, Bento admission, dock clearance, output scale,
   and passive-neighbor isolation.
6. Read-only controller registration and startup wiring; with explicit authorization,
   disable/re-enable and confirm complete desktop recovery.

Do not run live repair merely to satisfy this checklist. Use its disposable isolated
test unless repair behavior itself changed.

## Refactor rule

A regression blocks the refactor. A new product idea belongs in `ROADMAP-CC.md`
only after scope approval. Structural extraction must keep one mutable state owner,
typed controller boundaries, value-first admission, and rendering as a reader.
