# Kadunce 1.1 concept — Table

**Status:** J-authored post-1.0 product direction, logged September 15, 2026.
This is not an active engineering packet and does not expand the Itasca 1.0
release-critical path. Repo architecture remains authoritative when feasibility
and implementation are eventually opened.

## Product brief

Target: Post-1.0
Working name: Table
Scope: Spatial interface for existing KDE Plasma Virtual Desktops
Priority: Explicitly not part of Itasca 1.0 or current release-critical work.
Product idea
Table extends Kadunce's existing spatial interaction model outward to KDE Virtual Desktops.
Kadunce should not replace KDE Virtual Desktops or implement its own workspace backend. KDE/KWin remains authoritative for desktop membership, switching, lifecycle, and persistence.
Table is the interaction layer that makes those existing desktops feel tangible.
The basic hierarchy becomes:
Active = this window
Card Line / Bento = these windows
Table = these workspaces

Conceptually, the user is zooming outward through the same spatial system:
Window → Cards → Table
Primary interaction
From Card Line, a four-finger swipe upward transitions into Table.
Rather than presenting virtual desktops as abstract numbered thumbnails, Table lays them out as physical workspace surfaces containing representations of their Kadunce cards.
Example:
             TABLE

    Desktop 1       Desktop 2       Desktop 3

   ┌───┐ ┌───┐      ┌───┐         ┌───┐ ┌───┐
   │ A │ │ B │      │ C │         │ D │ │ E │
   └───┘ └───┘      └───┘         └───┘ └───┘
The user can directly grab a card and move it from one desktop to another.
No context menu. No "Move to Desktop…" workflow.
Pick up the window. Put it where you want it.
Entering and leaving Table
Proposed interaction grammar:
Four-finger swipe up from Card Line
→ Card Line recedes into Table.
Tap a desktop
→ Table dives into that desktop and returns to its Card Line.
Four-finger swipe down / dismiss gesture
→ Return to the originating desktop without changing context.
Potential future behavior:
Four-finger horizontal gesture
→ move directly between virtual desktops.
Do not claim or implement the horizontal gesture until existing Plasma/KWin gesture ownership and conflicts have been audited.
Direct manipulation
The primary purpose of Table is moving windows between workspaces.
A card should be draggable between desktop surfaces using the same physical/direct-manipulation philosophy already established by Kadunce.
Desired behavior:
1. Enter Table.
2. Grab a card from Desktop A.
3. Drag/toss it onto Desktop B.
4. KDE virtual-desktop membership updates underneath.
5. Table immediately reflects the new state.
6. Tap Desktop B.
7. Enter its Card Line with the moved window present.
Kadunce should call existing KWin/Plasma virtual-desktop capabilities rather than maintaining parallel desktop membership.
Visual philosophy
Table is not intended to become Plasma Overview with different styling.
It should feel like a physical table containing several piles of cards.
Desktops are spatial containers.
Windows remain recognizable as Kadunce cards rather than becoming tiny screenshot thumbnails wherever practical.
The transition is important to the mental model. Card Line should appear to move away from the user and become one workspace on the larger Table, preserving spatial continuity rather than abruptly switching screens.
Likewise, selecting another desktop should visually move into that workspace.
Touch-first design
Table should be designed around tablet interaction first while remaining fully usable with mouse/trackpad.
Four fingers intentionally signify manipulation of the workspace layer, distinguishing it from gestures affecting an individual card.
Touch targets must remain generous enough that moving cards between desktops does not require precision dragging.
Dragging toward a workspace should provide obvious destination feedback before the card is released.
Existing Kadunce architecture remains authoritative
Table must build on the stable Kadunce ownership model rather than reopening it.
Existing concepts remain intact:
- logical window/card identity
- Card Line
- Active
- Bento
- display ownership
- tablet ownership
- KWin integration
Table adds virtual-desktop scale above those systems.
It must not introduce a second window identity system, second virtual-desktop implementation, or competing workspace persistence layer.
Desktop/display relationship
Virtual desktops and physical displays are different dimensions and should remain so.
Table represents workspace membership, not monitor topology.
Multi-display behavior needs an explicit design/engineering pass before implementation so Table does not accidentally undo the cross-display ownership model already stabilized in Kadunce.
The existing connected-display/tablet behavior must remain a regression boundary.
Initial 1.1 acceptance concept
A successful first implementation should prove one complete physical interaction:
On a touch device, enter Card Line → four-finger swipe up → see multiple existing KDE Virtual Desktops as Table surfaces → drag a real Kadunce-managed window from one desktop to another → select the destination desktop → arrive in its Card Line with the window correctly transferred.

KDE should still report the correct underlying virtual-desktop membership.
Normal KDE desktop switching must continue working.
Existing Active, Card Line, Bento, monitor transfer, tablet ownership, and dock behavior must not regress.
Explicit non-goals
Table 1.1 is not:
- a replacement for KDE Virtual Desktops
- a replacement for KWin Overview
- a new Activities implementation
- a new window-management backend
- a reason to reopen solved Kadunce ownership architecture
- part of the Itasca 1.0 release
- part of the current installer/website critical path
Release sequencing
Itasca 1.0 ships first.
Current feature completion, design-system application, regression testing, packaging/bundle installation, and public website/release remain higher priority.
Table enters engineering only after 1.0 is stable enough that Kadunce can safely reopen for feature development.
North star
Active is the window. Card Line is the workspace. Table is the world above it.

Kadunce should make moving between all three scales feel like manipulating the same physical system, not navigating three separate interfaces.
