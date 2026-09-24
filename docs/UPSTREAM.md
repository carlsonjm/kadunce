# What Shuffle could give back to KDE

KDE problems the suite found or worked around. Nothing has been sent; J picks
what goes, as a report or a patch under the project's name. Check each against
current KDE code first. Evidence: **measured**, **source** (read from KDE's
code) or **inferred**.

## Ready or nearly ready

| What goes wrong for a person | Component | Evidence | Send as |
| --- | --- | --- | --- |
| After a drag is cut short with the finger down, the next touch drag moves no window and a tap can be lost. | KWin move filter | measured; `patches/kwin/0001-*.patch` | the patch |
| The keys pop up whenever an app enables a field soon after any touch: a tab switch, a new window. | KWin input method | source; hand tests 24 September | report, or a patch showing keys only for a touch on the field |
| Turning automatic keys off also stops explicit requests for them. | KWin input method | source | report |
| A raise request does nothing until some field has been touched once in the session. | KWin input method | measured | report |
| When the space below the keys grows, they stay floating where they were placed. | KWin input panel | measured | report or small patch |
| An overlay opened after the keys covers them. | KWin stacking | measured | report |
| A task list hiding hidden windows loses every minimized one on Wayland. | libtaskmanager | measured | report; may be deliberate |
| Key sounds can never be switched on: the build option is `PLASMA_KEYBOARD_SOUNDS_ENABLED`, but the code reads `PLASMA_KEYBOARD_SOUND_ENABLED`. | Plasma Keyboard | source | one-line patch |
| A reinstalled effect reports loaded while the old build runs until logout. | KWin effect loading | measured | report |
| With a monitor made primary, the tablet's desktop goes black, Kadunce off too. | plasma-workspace | measured once | report once reproduced |

## Missing capabilities

Feature requests, or small patches: the task manager's menu, icon placement and
badges reusable by another dock; Meta and a number for any task applet; knowing
when a window touches a panel; an effect learning a touchscreen's display and
whether a window is a layer surface; a touch-cancel callback for input spies;
edge tiling off for one move; and, from the Keyboard fork, shortcuts, raising
with no text field and an adjustable height.

## Not yet understood

A maximize and minimize in one call leaving a short frame; a captionless window
at the tablet's bottom; the handle broken after a monitor unplug; a layer
surface keeping the wrong width.
