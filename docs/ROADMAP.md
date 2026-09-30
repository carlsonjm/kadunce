# Roadmap

What is planned for Kadunce, and what it does not do yet.

## Planned

- In Bento, replacing a pane by dropping a window on its side.
- Monitor layouts past eight panes, and a window KWin moves to a monitor by
  other means joining that monitor's layout.
- Panes and ordinary windows making room for the on-screen keys, as the Active
  card does.
- The on-screen keys coming up for the first text field of a session, and for a
  tap just after a card is chosen.
- Motion that follows the system's animation speed and reduced-motion setting
  everywhere.
- Touch monitors beyond a tablet's own panel, tested on real hardware.

## Known limitations

- A display change is answered once KWin has moved windows for it. A window
  sent to another display some other way, such as KWin's own window-to-screen
  shortcut, is not answered.
- Whether switching Kadunce off returns a window to the right place after its
  display has moved in the desktop layout is not measured.
- A card moved to another virtual desktop with the window menu is not handled.
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
