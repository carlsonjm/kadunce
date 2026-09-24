# What Shuffle could give back to KDE

Problems in KDE software that Shuffle found, measured or worked around, and that
would help other Plasma users if KDE fixed them. Nothing here has been sent. J
decides what goes, and each item goes as a bug report or a patch under the
project's name. Before sending, check the item against current KDE code: it may
already be fixed.

Evidence is **measured** (reproduced by test or by hand), **source** (read from
KDE's code) or **inferred**.

## Ready or nearly ready

| What goes wrong for a person | Component | Evidence | Send as |
| --- | --- | --- | --- |
| After a window drag is cut short with the finger still down, the next touch drag does not move a window and the tap after it can be lost. | KWin move filter | measured; `patches/kwin/0001-bound-native-touch-to-move-lifetime.patch` | the patch, as a merge request |
| The on-screen keys pop up whenever an app activates a text field shortly after any touch: a tab switch, a new window, a card brought forward. This is what made KDE's keyboard unusable on the tablet. | KWin input method, `shouldShowOnActive` | source, and hand tests on 24 September | a bug report; a patch that shows the keys only for a touch on the field |
| Turning automatic keys off also stops every explicit request for them, because the panel is never allowed to show. | KWin input method | source | a bug report, with the item above |
| A request to raise the keys does nothing until some text field has been touched once in the session. | KWin input method | measured, 22 September | a bug report |
| When the space below the keys grows, the keys stay where they were placed, floating above the bottom. | KWin input panel placement | measured by the Keyboard's seat test | a bug report or a small patch |
| An overlay opened after the keys covers them, so every key tap goes to the overlay. | KWin layer stacking | measured, 21 September | a bug report |
| A task list that hides hidden windows loses every minimized window on Wayland, because hidden is read from minimized. | libtaskmanager | measured in a private compositor | a bug report; it may be deliberate |
| Clicking an app with several windows can bring nothing forward when the shell started after those windows were last used. | Icons-only Task Manager | source; the same gap measured in the Shuffle Dock | a small patch |
| A freshly installed effect reports loaded while the previous build keeps running until logout. | KWin effect loading | measured | a bug report |
| After a monitor is made primary, the tablet's desktop goes black and a new wallpaper does not reach it, with Kadunce off too. | plasma-workspace | measured once; not yet reproduced | a bug report once reproduced |

## Missing capabilities

Each of these made Shuffle copy KDE code or work around a gap. Each is a feature
request, or a patch where the change is small.

- A third-party dock cannot reuse the task manager's menu, icon placement or
  unread badges; they are compiled into the applet.
- Meta and a number reach only an applet that declares it manages tasks.
- Only the shell's own panel knows when a window touches it.
- An effect cannot learn which display a touchscreen was matched to, or ask
  whether a window is a layer surface.
- Input spies get no touch-cancel callback, and move-start does not say which
  device started it.
- Edge tiling cannot be turned off for one window or one move.
- Plasma Keyboard cannot send shortcuts, cannot be raised with no text field,
  and has a fixed height. The Shuffle Keyboard fork adds all three.

## Not yet understood

These need their owner narrowed down before they can be reported: a maximize and
minimize in one call leaving a short restored frame; a captionless window at the
tablet's bottom; the keys' handle left broken after a monitor unplug; a layer
surface keeping the wrong width after a resize.
