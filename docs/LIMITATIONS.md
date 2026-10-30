# Known limits

What Kadunce does not do yet, and what it will not do.

- Kadunce reacts to a display change after KWin has moved the windows for it.
  When an ordinary window reaches another display some other way, such as
  KWin's own window-to-screen shortcut, Kadunce treats it as a window that
  opened there. The Active card sent that way stops being a card and stays
  where KWin put it, outside that display's layout. Kadunce ignores a pane sent
  that way.
- Bringing applications back places only each application's first window. A
  pair on a workspace not shown comes back as two cards, and cards return in
  the order their windows arrive rather than the order they stood.
- Nobody has measured whether switching Kadunce off returns a window to the
  right place after its display has moved in the desktop layout.
- A Wayland dialog that names no parent window becomes a card. No application
  surveyed so far does this.
- The plugin is built for the KWin it is installed against, and KWin loads it
  only for that exact KWin version, so it needs a rebuild after every KWin
  update. A rebuilt plugin may need a logout and login before the compositor
  loads it.
- Guided repair rebuilds the source kept at installation, runs its tests and
  asks for authorization to replace one plugin file, through the root-owned
  helper Shuffle installs where it is set up, otherwise the system's own
  `/usr/bin/install`. It works offline, so the build
  dependencies must already be installed. Its log is
  `$XDG_STATE_HOME/kadunce/repair.log`, normally
  `~/.local/state/kadunce/repair.log`. Its checksums catch accidental edits but
  cannot stop someone who already controls the account.
