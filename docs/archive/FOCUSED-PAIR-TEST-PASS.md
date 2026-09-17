# Focused pair acceptance

User accepted the final direct guest replacement on 2026-09-10 and authorized
freezing and pushing the day's updates. The follow-up sections below record the
issues found during testing and their corrections; the final handoff was passed.

With two logical groups, the centered preview uses 64% of the work area. Its
peeking neighbor retains standard 54% dimensions and vertical alignment, with
the same aspect ratio and 5.6% shoulder gap as standard Card Line. On a swap,
the incoming preview grows to 64% and the outgoing preview shrinks to 54%.
Three or more retain the existing 54% layout, so the surviving shoulder only
changes horizontal position on third-card arrival. Group members are not
counted as extra slots. No real-window resize
is performed by the preview transition.

Paging toward the exposed partner swaps the two groups over 280 ms (OutCubic).
The outgoing center stays on the side the swipe leaves behind. Paging toward the
empty shoulder does nothing in pair overview; ordinary Active navigation remains
unchanged. A reversal captures the current interpolated positions.

New application admission now selects the newcomer at center. The old primary
moves to the opposite side from the existing shoulder, which retains its side.
After a 280 ms settle, the newcomer expands toward Active for 220 ms. Promotion
waits for a shown, drawable window, not for application content to finish loading.
If the window never becomes drawable within ten seconds, the overview remains;
there is no fallback to the old app. Selecting a different app, paging, grabbing,
opening Tette, or releasing the effect cancels pending automatic promotion.

Tette uses 64% with zero/one neighboring groups and 54% with two or more. Its
neighbors remain 54%. The guest footprint is held for the lease, including while
an application is being admitted. Opening Tette animates the old primary aside
without crossing an existing pair shoulder to the other side. Standalone Tette
uses 64% of its screen's available area. After a committed guest swipe,
the landing eases into the larger pair without reversing its shoulder.
Active entry, release and carried-card handling cancel preview interpolation.

## Live acceptance

1. Verify the persistent tray kill switch before testing and after logout/login.
2. Two standalone apps: verify fully visible larger center and one exposed edge.
3. Swipe the partner into center, then reverse; repeat quickly during motion.
   Check that there is no cross-screen teleport, duplicate or blank center.
4. Launch a new app from the dock: verify it takes center, the old primary moves
   aside, then the newcomer expands into Active. Repeat with the partner on the
   opposite side. During arrival, choose another card and check no delayed steal.
5. Close a side app from a three-group line; check the surviving shoulder and
   expansion. Close a stack member: it must not count as a group departure.
6. Two groups with multiple members: check fan clearance, selection and dragging.
7. Tette with zero, one and two groups: verify 64/64/54% sizing. With two groups,
   swipe toward either neighbor, then reverse pair paging. Launch a new app from
   Tette and verify its center-to-Active arrival after the guest disappears.
8. Recheck dock activation, Active gutter, fullscreen release and kill switch.

Automated checks cover repeated model paging, group count, third arrival/removal
on both sides, and centered/exposed pair geometry on landscape and portrait work
areas (including offset outputs). They do not prove compositor motion quality.

## Dock input follow-up

The user accepted the 64% center/54% shoulder presentation, but reported a dock
launch returning to the last Active card. The router captured all tablet input
in Card Line; a click aligned horizontally with center could activate that card
even when it was on Plasma's panel. Panel hit testing now uses KWin's shown dock
input surface, with no fixed screen-edge strip. Mouse transactions starting on
the panel remain passed through until release; panel hover/wheel also pass.
Overview touch taps on the panel remain unowned. Existing card gestures keep
ownership when crossing the panel; Active/Inactive system-edge touch behavior
is unchanged.

The panel-input test covers ordinary overview and Tette, pointer press/release
across the panel/card boundary, hover/wheel, touch across the same boundary, and
normal center-card activation. Live test pending: launch a closed dock app and
activate an existing dock app, both with and without Tette present. Confirm that
the requested app—not the prior selected card—becomes Active, and that the tray
kill switch still accepts both mouse buttons.

## Direct guest replacement follow-up

The user passed the remaining behavior, but one-app → Tette → Zen was routing
Zen through a shoulder and GPT through center. The guest now retains references
to its original neighbor windows while a new application is admitted. A newly
admitted window cannot become a guest shoulder merely by changing model order.
Once the matched window is drawable, it is selected and seeded at the guest's
center aperture before notifying Tette to fade. Existing previews retain their
painted origins; a single neighbor retains its physical side. Ending the lease
does not replay a guest swipe or reselect the previous primary. Existing arrival
settle/expansion timing, dock input and card dimensions are unchanged.

Live recheck: GPT alone → Tette → cold launch Zen. GPT must stay on its shoulder,
Zen must replace Tette at center, then expand. Repeat with two neighboring groups
and swipe-cancel once to verify the ordinary guest-navigation path is intact.
