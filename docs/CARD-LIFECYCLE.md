# Card Lifecycle — Canonical Product Contract

## 1. Core model

Every eligible application window has exactly one owner:

1. **Native**

   - Plasma owns the window.
   - It is outside Kadunce.

2. **Individual card**

   - Kadunce owns the window as an independent card.
   - It may be Active, visible in Card Line, sleeping, or hidden behind another
     selected state.

3. **Bento pane**

   - Kadunce owns the window as part of the display's current visible Bento
     combination.
   - It appears inside the single Bento group card in Card Line.

A window cannot be an individual card and a Bento pane at the same time.

Leaving Bento removes all previous Bento association.

## 2. Presentation states

Ownership and presentation are separate.

### Active

One individual card is shown as the current window.

Other cards remain owned and hidden.

### Bento

The display's current Bento pane combination is shown.

Individual cards remain owned and hidden.

### Card Line

Card Line shows:

- Every individual card
- Every ordinary card stack
- The display's single Bento group, if Bento exists

The Bento group is one Card Line entry regardless of its pane count.

### Sleeping

A minimized individual card remains owned but is not presented.

Selecting it wakes it and presents it as Active.

### Native desktop

The window has been explicitly released from Kadunce.

Kadunce no longer owns or hides it.

## 3. Display admission

Enabling Kadunce alone does not capture windows.

The first deliberate Card or Bento action starts Kadunce ownership for that display
and current virtual desktop.

Kadunce atomically adopts every eligible open application window on that display.

### Top-edge entry

- The dragged window becomes an individual card.
- It becomes Active.
- Every other eligible window becomes an individual nonselected card.

### Left/right-edge entry

- Kadunce creates or updates the display's Bento layout.
- The selected window enters the requested Bento side.
- Other eligible windows may fill the remaining visible panes.
- Windows outside the visible pane combination become individual cards.

If the complete ownership batch cannot be prepared safely, every window remains
Native.

## 4. Eligible windows

Kadunce adopts normal top-level application windows on the current display and
current virtual desktop.

Kadunce does not create independent cards for:

- Panels or docks
- Desktop surfaces
- Tooltips or menus
- Temporary popups
- Child dialogs that must follow a parent
- Windows explicitly excluded from normal task switching

A dependent dialog follows its owning application card.

Other displays and virtual desktops remain independent.

## 5. Bento rules

Each display can have only one live Bento layout.

Bento owns only the windows currently visible in its pane combination.

Bento does not own hidden overflow.

Bento does not retain minimized or displaced panes.

### When a pane leaves Bento

The window becomes an independent Card Line card when it is:

- Dragged to the top edge
- Minimized
- Displaced by a new pane combination
- Removed because it no longer fits
- Excluded from the current visible layout

It retains no saved Bento position or group association.

### One remaining pane

When Bento falls to one visible pane, Bento ends.

The remaining pane becomes an individual card.

### Returning a card to Bento

An individual card returns to Bento only through an explicit left/right-edge action.

If the Bento combination is full, the displaced pane becomes an individual card.

## 6. Card Line selection

### Selecting an individual card

- That card becomes Active.
- The Bento group remains owned and hidden.
- Other individual cards remain owned and hidden.
- No window returns to the native desktop.

### Selecting the Bento group

- Only the Bento group resumes.
- Individual cards remain independently owned and hidden.
- No individual card becomes a Bento member.
- No individual card returns to the native desktop.

### Returning to Card Line

The same peer entries return:

- Independent cards remain independent.
- Ordinary stacks preserve their membership.
- Bento returns as one grouped entry.
- Selection remains on the most recently presented entry.

Changing presentation never consumes restoration data.

## 7. Minimize behavior

### Minimizing an individual card

- It remains an individual card.
- It becomes sleeping and nonselected.
- Its user-minimized intent is preserved.

### Minimizing a Bento pane

- It immediately leaves Bento.
- It becomes a sleeping individual card.
- Bento reflows its remaining visible panes.
- Selecting the sleeping card wakes it as Active.

A minimized card never silently returns to Bento.

## 8. New windows

### While Bento is presented

Kadunce attempts to admit the new window into the visible Bento combination.

- If it fits, Bento recomposes.
- Any displaced pane becomes an individual card.
- If it cannot fit safely, the new window becomes an individual Active card.
- Existing Bento remains a separate hidden group.

### While an individual card is Active

The new window becomes a new individual Active card.

The previous Active card remains an individual Card Line neighbor.

### While Card Line is open

The new window becomes an individual card and becomes the selected Card Line entry.

It does not silently join Bento unless the user sends it through a Bento edge.

## 9. Ordinary stacks

Individual cards may be organized into ordinary Card Line stacks.

A stack:

- Has one selected member
- Remains separate from Bento
- Can page through its members
- Can release a member back into the Card Line
- Shows its active position in the label row

A Bento group:

- Is not an ordinary stack
- Cannot page its pane members
- Cannot be inserted into another stack
- Cannot retain removed panes
- Shows the names of its currently visible applications

## 10. Edge actions

### Top edge

Make the carried window an independent Active card.

### Left or right edge

Admit the carried window into Bento.

### Bottom edge

Release the carried window to the ordinary Plasma desktop.

### Cancel or Escape

Return the window to its exact ownership and presentation state from before the
carry.

An edge action commits only when released inside its valid edge zone.

## 11. Multiple displays

Each display has an independent ownership session.

Each display may contain:

- Individual cards
- Ordinary stacks
- At most one Bento layout

Moving a card between displays uses destination-first transfer:

1. Prepare the destination.
2. Validate the destination.
3. Move the ownership and restoration record.
4. Remove the source ownership.

A failed transfer changes neither display.

## 12. Window closure

Closing a window removes its card identity.

### Closing an individual card

The card disappears from Card Line.

### Closing a Bento pane

Bento reflows.

If one pane remains, Bento ends and that pane becomes an individual card.

### Closing the Active card

Kadunce returns to Card Line when other owned cards remain.

If nothing remains, the display's Kadunce session ends.

## 13. Release and disable

Explicit bottom-edge release affects only the carried window.

Full Kadunce release or disable:

- Restores every owned window exactly once
- Restores original geometry
- Restores output and virtual desktop
- Restores quick-tile, maximize, fullscreen, and minimized state
- Clears individual cards, stacks, Bento, and presentation state
- Returns complete authority to Plasma

Kadunce does not persist card or Bento membership across unload or restart.

## 14. Required invariants

- One window has one owner.
- One display has at most one Bento layout.
- Bento owns visible panes only.
- A window outside the visible Bento combination is an individual card.
- Active is a presentation state, not a separate ownership class.
- Selecting one entry never releases its neighbors.
- Presentation changes never overwrite restoration records.
- Destination acceptance happens before source removal.
- Failed or cancelled transitions preserve the exact prior state.
- Other displays and virtual desktops remain untouched.
- Only explicit release or disable returns a managed window to the native desktop.

## 15. Shuffle navigation

- Bottom-edge swipe up launches Card Spread.
- Top-edge swipe down returns to the current card state:
  - Solo Card
  - Card Stacks
  - Bento, with its layout resumed
