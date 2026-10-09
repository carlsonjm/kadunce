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
- Spread named for a screen reader, as Table is, and checked with Orca.
- Touch monitors beyond a tablet's own panel, tested on real hardware.
- A placement request (`REQUESTS.md`) taking a pane out of its layout.
- The identifiers still outside `co.goodinput.*`, Tettegouche's desktop entries
  and Gooseberry's notes interface, and the `showCardLine` method and `cardLine`
  value Tettegouche reads, renamed with a versioned migration.

## Known limitations

- A display change is answered once KWin has moved windows for it. An
  ordinary window sent to another display some other way, such as KWin's own
  window-to-screen shortcut, is answered as one that opened there; a card or
  pane sent that way is not.
- Whether switching Kadunce off returns a window to the right place after its
  display has moved in the desktop layout is not measured.
- A Wayland dialog that names no parent window becomes a card. No application
  surveyed so far does this.
- The plugin is built for the KWin it is installed against, and KWin loads it
  only for that exact KWin version, so it needs a rebuild after every KWin
  update. A rebuilt plugin may need a logout and login before the compositor
  loads it.
- Guided repair rebuilds the source kept at installation, runs its tests and
  asks for authorization to replace one plugin file. It downloads nothing and
  needs the build dependencies. Its log is
  `$XDG_STATE_HOME/kadunce/repair.log`, normally
  `~/.local/state/kadunce/repair.log`. Its checksums catch accidental edits, not
  someone who already controls the account.
