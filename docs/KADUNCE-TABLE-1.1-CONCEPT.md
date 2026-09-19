# Table product contract

**Status:** Required for Shuffle 1.0; engineering feasibility queued.

Table extends Kadunce's spatial model to existing KDE Plasma virtual desktops:

`Active = this window → Spread/Bento = these windows → Table = these workspaces`

KWin remains authoritative for virtual-desktop membership, switching, lifecycle,
and persistence. Table is the interaction and presentation layer.

## Primary interaction

1. Enter Table from Spread with a four-finger upward gesture.
2. See existing virtual desktops as physical workspace surfaces containing
   recognizable Kadunce cards.
3. Drag a real card from one desktop surface to another.
4. Update the underlying KWin virtual-desktop membership.
5. Select the destination desktop and return to its Spread with the moved window.
6. Dismiss downward to return to the origin without changing context.

Four fingers signify workspace-level manipulation and distinguish Table from
single-card gestures. A horizontal desktop-switch gesture is unapproved until KWin
and Plasma gesture conflicts are audited.

## Presentation

Spread should appear to recede into one surface on a larger table. Selecting a
desktop should move back into that workspace, preserving the sense of one spatial
system. Desktop surfaces must remain large enough for touch manipulation and show a
clear destination state before release.

Table is a workspace view rather than a restyled Plasma Overview. Windows remain
Kadunce cards where practical; desktop surfaces are spatial containers rather than
numbered thumbnail strips.

## Invariants

- Use KWin's virtual-desktop APIs; do not create parallel desktop membership.
- Reuse Kadunce's window identity and prepared-transfer rules.
- Keep virtual desktops and physical outputs as independent dimensions.
- Preserve Active, Spread, Bento, monitor transfer, tablet ownership, dock
  behavior, restoration, and the disable control.
- Normal KDE desktop switching must continue to work.
- A rejected or interrupted transfer preserves the source desktop and card state.

## Feasibility gate

Before product implementation:

- audit current KWin/Plasma virtual-desktop and gesture APIs;
- define state ownership across desktop switch, output change, client close, effect
  unload, and session restore;
- prove one value-first transfer on a private prototype;
- physically test touch entry, card transfer, destination selection, and dismissal;
- verify the underlying desktop membership through KDE independently of Table.

## 1.0 acceptance

On a supported touch device, the user can enter Table, see multiple existing KDE
virtual desktops, move a real Kadunce-managed window between them, enter the
destination Spread, and observe correct underlying membership. Existing Kadunce
and KDE behavior remains intact.

## Non-goals

- Replacing KDE virtual desktops, KWin Overview, or Activities.
- A second window manager or persistence layer.
- Reopening stable Kadunce ownership architecture.
- Cross-desktop features beyond the proven direct-manipulation interaction.
