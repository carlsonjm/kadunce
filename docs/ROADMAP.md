# Roadmap

What is planned for Kadunce, and what it does not do yet.

## Planned

- A card or pane KWin moves to a monitor by other means joining that
  monitor's layout.
- A display unplugged and plugged back in getting its drawn zones' panes back
  in place, as KWin itself does.
- Panes and ordinary windows making room for the on-screen keys, as the Active
  card does.
- The on-screen keys coming up for the first text field of a session, and for a
  tap just after a card is chosen.
- Under a reduced-motion preference, where the platform exposes one apart
  from Plasma's instant speed, short fades in place of travel.
- Touch monitors beyond a tablet's own panel, tested on real hardware.
- One package and interface namespace across the suite, with a documented
  migration from today's identifiers.
- From Spread, a pull from the top edge bringing Table's tabs over Spread, which
  stays showing; the full-size previews come only past the tabs.
- A placement request (`REQUESTS.md`) taking a card onto another display, a
  window from another display onto the card display, and a pane out of its
  layout.

## Known limitations

- A display change is answered once KWin has moved windows for it. An
  ordinary window sent to another display some other way, such as KWin's own
  window-to-screen shortcut, is answered as one that opened there; a card or
  pane sent that way is not.
- Whether switching Kadunce off returns a window to the right place after its
  display has moved in the desktop layout is not measured.
- A Wayland dialog that names no parent window becomes a card. No application
  surveyed so far does this.
- The plugin is built for the KWin it is installed against and needs a rebuild
  after a KWin update changes KWin's plugin interface. A rebuilt plugin may need
  a logout and login before the compositor loads it.
- Guided repair rebuilds the source kept at installation, runs its tests and
  asks for authorization to replace one plugin file. It downloads nothing and
  needs the build dependencies. Its log is
  `$XDG_STATE_HOME/kadunce/repair.log`, normally
  `~/.local/state/kadunce/repair.log`. Its checksums catch accidental edits, not
  someone who already controls the account.
